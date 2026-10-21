#pragma once

#include "LowLevelTask.h"
#include "WorkStealingQueue.h"
#include <vector>
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <memory>

namespace Tasks
{
    // 스레드별 실행 통계 정보
    struct FThreadExecutionStats
    {
        int32_t WorkerIndex = -1;
        uint32_t TasksExecuted = 0;
        float BusyMs = 0.0f;
    };

    // 워커 스레드 및 스케줄링 관리자
    class FTaskScheduler
    {
    public:
        static FTaskScheduler& Get();

        void Initialize(uint32_t InNumWorkers = 0);
        void Shutdown();

        bool IsRunning() const { return bIsRunning.load(std::memory_order_relaxed); }
        uint32_t GetNumWorkers() const { return NumWorkers; }
        int32_t GetCurrentWorkerIndex() const;

        // 프레임 통계 갱신 및 조회
        void BeginFrame();
        void GetThreadStats(std::vector<FThreadExecutionStats>& OutStats) const;

        // 태스크 디스패치
        void Schedule(const FLowLevelTask& Task);

        // 일감 하나 처리 시도
        bool ExecuteOneTask();

        // 조건 만족까지 대기하며 일감 대리 처리
        void HelpSteal(const std::function<bool()>& StopCondition);

    private:
        FTaskScheduler() = default;
        ~FTaskScheduler();

        void WorkerLoop(int32_t WorkerIndex);

    private:
        static constexpr size_t MaxTrackedThreads = 32;
        std::atomic<uint32_t> FrameTasksExecuted[MaxTrackedThreads]{};
        std::atomic<uint64_t> FrameBusyMicroseconds[MaxTrackedThreads]{};
        std::vector<FThreadExecutionStats> CachedThreadStats;

    private:
        // Initialize/Shutdown are called by the engine thread, with external producers stopped.
        std::atomic<bool> bIsRunning{ false };
        std::atomic<uint64_t> PendingTasks{ 0 };
        uint32_t NumWorkers = 0;

        std::vector<std::thread> Workers;
        std::vector<std::unique_ptr<FWorkStealingQueue>> WorkerQueues;

        std::mutex GlobalQueueMutex;
        std::deque<FLowLevelTask> GlobalQueue;

        std::mutex WakeMutex;
        std::condition_variable WakeCondition;
        std::atomic<int32_t> SleepingWorkerCount{ 0 };
    };
}
