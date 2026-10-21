#include "EnginePCH.h"
#include "TaskScheduler.h"
#include <algorithm>
#include <chrono>

namespace Tasks {
// 스레드 로컬 워커 식별자
static thread_local int32_t TLS_WorkerIndex = -1;

FTaskScheduler &FTaskScheduler::Get() {
  static FTaskScheduler Instance;
  return Instance;
}

FTaskScheduler::~FTaskScheduler() { Shutdown(); }

int32_t FTaskScheduler::GetCurrentWorkerIndex() const {
  return TLS_WorkerIndex;
}

void FTaskScheduler::Initialize(uint32_t InNumWorkers) {
  if (bIsRunning.load(std::memory_order_relaxed)) {
    return;
  }

  if (InNumWorkers == 0) {
    uint32_t HardwareThreads = std::thread::hardware_concurrency();
    NumWorkers = HardwareThreads > 1 ? HardwareThreads - 1 : 1;
  } else {
    NumWorkers = InNumWorkers;
  }

  WorkerQueues.clear();
  WorkerQueues.reserve(NumWorkers);
  for (uint32_t Index = 0; Index < NumWorkers; ++Index) {
    WorkerQueues.push_back(std::make_unique<FWorkStealingQueue>());
  }

  bIsRunning.store(true, std::memory_order_release);

  Workers.clear();
  Workers.reserve(NumWorkers);
  for (uint32_t Index = 0; Index < NumWorkers; ++Index) {
    Workers.emplace_back(&FTaskScheduler::WorkerLoop, this,
                         static_cast<int32_t>(Index));
  }
}

void FTaskScheduler::Shutdown() {
  if (!IsRunning()) return;
  // Keep workers running while draining, including dependent tasks scheduled on completion.
  HelpSteal([this] { return PendingTasks.load(std::memory_order_acquire) == 0; });
  bIsRunning.store(false, std::memory_order_release);

  {
    std::lock_guard<std::mutex> Lock(WakeMutex);
    WakeCondition.notify_all();
  }

  for (std::thread &Worker : Workers) {
    if (Worker.joinable()) {
      Worker.join();
    }
  }
  Workers.clear();
  WorkerQueues.clear();

  std::lock_guard<std::mutex> Lock(GlobalQueueMutex);
  GlobalQueue.clear();
}

void FTaskScheduler::BeginFrame() {
  CachedThreadStats.clear();
  CachedThreadStats.reserve(NumWorkers + 1);

  // 메인 스레드 통계 수집
  {
    FThreadExecutionStats MainStat{};
    MainStat.WorkerIndex = -1;
    if (NumWorkers < MaxTrackedThreads) {
      MainStat.TasksExecuted = FrameTasksExecuted[NumWorkers].exchange(0, std::memory_order_relaxed);
      MainStat.BusyMs = FrameBusyMicroseconds[NumWorkers].exchange(0, std::memory_order_relaxed) / 1000.0f;
    }
    CachedThreadStats.push_back(MainStat);
  }

  // 워커 스레드 통계 수집
  for (uint32_t Index = 0; Index < NumWorkers; ++Index) {
    FThreadExecutionStats WorkerStat{};
    WorkerStat.WorkerIndex = static_cast<int32_t>(Index);
    if (Index < MaxTrackedThreads) {
      WorkerStat.TasksExecuted = FrameTasksExecuted[Index].exchange(0, std::memory_order_relaxed);
      WorkerStat.BusyMs = FrameBusyMicroseconds[Index].exchange(0, std::memory_order_relaxed) / 1000.0f;
    }
    CachedThreadStats.push_back(WorkerStat);
  }
}

void FTaskScheduler::GetThreadStats(std::vector<FThreadExecutionStats>& OutStats) const {
  OutStats = CachedThreadStats;
}

void FTaskScheduler::Schedule(const FLowLevelTask &Task) {
  // Before initialization, execute synchronously rather than leaving Wait unfinished.
  if (!IsRunning()) { Task.Execute(); return; }
  PendingTasks.fetch_add(1, std::memory_order_acq_rel);
  const int32_t CurrentWorker = TLS_WorkerIndex;

  // 현재 스레드가 워커인 경우 로컬 큐에 우선 배치
  if (CurrentWorker >= 0 &&
      static_cast<size_t>(CurrentWorker) < WorkerQueues.size()) {
    if (WorkerQueues[CurrentWorker]->Push(Task)) {
      if (SleepingWorkerCount.load(std::memory_order_relaxed) > 0) {
        std::lock_guard<std::mutex> Lock(WakeMutex);
        WakeCondition.notify_one();
      }
      return;
    }
  }

  // 로컬 큐가 가득 찼거나 외부 스레드인 경우 글로벌 큐에 배치
  {
    std::lock_guard<std::mutex> Lock(GlobalQueueMutex);
    GlobalQueue.push_back(Task);
  }

  {
    std::lock_guard<std::mutex> Lock(WakeMutex);
    WakeCondition.notify_all();
  }
}

bool FTaskScheduler::ExecuteOneTask() {
  const int32_t CurrentWorker = TLS_WorkerIndex;
  FLowLevelTask TaskToExecute{};
  bool bFoundTask = false;

  // 로컬 큐 우선 인출
  if (CurrentWorker >= 0 &&
      static_cast<size_t>(CurrentWorker) < WorkerQueues.size()) {
    if (WorkerQueues[CurrentWorker]->Pop(TaskToExecute)) {
      bFoundTask = true;
    }
  }

  // 글로벌 큐 인출
  if (!bFoundTask) {
    std::unique_lock<std::mutex> Lock(GlobalQueueMutex);
    if (!GlobalQueue.empty()) {
      TaskToExecute = GlobalQueue.front();
      GlobalQueue.pop_front();
      bFoundTask = true;
    }
  }

  // 타 워커 큐 강탈 시도
  if (!bFoundTask) {
    const size_t QueueCount = WorkerQueues.size();
    if (QueueCount > 0) {
      const size_t StartOffset =
          CurrentWorker >= 0 ? static_cast<size_t>(CurrentWorker + 1) : 0;
      for (size_t Step = 0; Step < QueueCount; ++Step) {
        const size_t TargetIndex = (StartOffset + Step) % QueueCount;
        if (static_cast<int32_t>(TargetIndex) == CurrentWorker) {
          continue;
        }

        if (WorkerQueues[TargetIndex]->Steal(TaskToExecute)) {
          bFoundTask = true;
          break;
        }
      }
    }
  }

  // 태스크 실행 및 계측
  if (bFoundTask && TaskToExecute.Function) {
    const auto StartTime = std::chrono::steady_clock::now();
    try { TaskToExecute.Execute(); }
    catch (...) { PendingTasks.fetch_sub(1, std::memory_order_release); throw; }
    const auto EndTime = std::chrono::steady_clock::now();
    const uint64_t ElapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime).count();

    const int32_t TrackIdx = (CurrentWorker >= 0 && CurrentWorker < static_cast<int32_t>(NumWorkers)) ? CurrentWorker : static_cast<int32_t>(NumWorkers);
    if (static_cast<size_t>(TrackIdx) < MaxTrackedThreads) {
      FrameTasksExecuted[TrackIdx].fetch_add(1, std::memory_order_relaxed);
      FrameBusyMicroseconds[TrackIdx].fetch_add(ElapsedUs, std::memory_order_relaxed);
    }
    PendingTasks.fetch_sub(1, std::memory_order_release);
    return true;
  }
  if (bFoundTask) PendingTasks.fetch_sub(1, std::memory_order_release);
  return false;
}

void FTaskScheduler::HelpSteal(const std::function<bool()> &StopCondition) {
  while (true) {
    if (StopCondition && StopCondition()) {
      break;
    }

    if (!ExecuteOneTask()) {
      std::this_thread::yield();
    }
  }
}

void FTaskScheduler::WorkerLoop(const int32_t WorkerIndex) {
  TLS_WorkerIndex = WorkerIndex;

  while (bIsRunning.load(std::memory_order_relaxed)) {
    if (ExecuteOneTask()) {
      continue;
    }

    // 처리할 일감이 없으면 대기 상태로 전환
    std::unique_lock<std::mutex> Lock(WakeMutex);
    SleepingWorkerCount.fetch_add(1, std::memory_order_relaxed);

    WakeCondition.wait_for(Lock, std::chrono::milliseconds(1));

    SleepingWorkerCount.fetch_sub(1, std::memory_order_relaxed);
  }
  TLS_WorkerIndex = -1;
}
} // namespace Tasks
