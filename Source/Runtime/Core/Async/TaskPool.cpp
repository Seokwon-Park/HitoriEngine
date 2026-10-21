#include "EnginePCH.h"
#include "TaskPool.h"

void FTaskPool::Init(uint32 NumWorkers)
{
    for (uint32 i = 0; i < NumWorkers; ++i)
        Workers.emplace_back([this] { WorkerLoop(); });
}

void FTaskPool::Shutdown()
{
    {
        std::lock_guard Lock(Mutex);
        bStop = true;
    }
    WakeCV.notify_all();
    for (std::thread& Worker : Workers) Worker.join();   // 각 워커가 루프를 빠져나올 때까지 대기
    Workers.clear();
}

void FTaskPool::ParallelFor(uint32 InCount, uint32 InChunkCount, const FChunkFunc& InFunc)
{
    if (InCount == 0) return;
    InChunkCount = std::clamp(InChunkCount, 1u, InCount);

    // 워커가 없거나 조각이 하나면 그냥 메인에서 실행
    if (Workers.empty() || InChunkCount == 1)
    {
        for (uint32 c = 0; c < InChunkCount; ++c)
            InFunc(uint64(InCount) * c / InChunkCount, uint64(InCount) * (c + 1) / InChunkCount, c);
        return;
    }

    {
        std::lock_guard Lock(Mutex);          // 작업 정보를 락 안에서 써야 깨어난 워커가 확실히 본다
        Func = &InFunc;
        Count = InCount;
        ChunkCount = InChunkCount;
        NextChunk.store(0);
        PendingWorkers.store(static_cast<uint32>(Workers.size()));
        ++Generation;
    }
    WakeCV.notify_all();                      // 전원 기상

    RunChunks();                              // 메인도 논다고 기다리지 말고 같이 처리

    // 모든 워커가 이번 작업에서 손을 뗄 때까지 기다린다. acquire: 워커들이 쓴 결과를 확실히 본다.
    while (PendingWorkers.load(std::memory_order_acquire) != 0)
        _mm_pause();                          // CPU에 "바쁜 대기 중"이라고 알려 전력과 경합을 줄임

    Func = nullptr;
}

void FTaskPool::WorkerLoop()
{
    uint64 SeenGeneration = 0;
    while (true)
    {
        {
            std::unique_lock Lock(Mutex);
            // 조건이 참이 될 때까지 잔다. 조건을 같이 넘기면 이유 없이 깨어나도(spurious wakeup) 다시 잔다.
            WakeCV.wait(Lock, [&] { return bStop || Generation != SeenGeneration; });
            if (bStop) return;
            SeenGeneration = Generation;      // 이 작업은 봤다
        }

        RunChunks();

        // 이번 작업에서 내 몫은 끝. release: 내가 쓴 결과가 메인에게 보이도록 보장한다.
        PendingWorkers.fetch_sub(1, std::memory_order_release);
    }
}

void FTaskPool::RunChunks()
{
    while (true)
    {
        // 원자적으로 "현재 값을 받고 +1". 두 스레드가 같은 번호를 받는 일이 없다.
        const uint32 Chunk = NextChunk.fetch_add(1);
        if (Chunk >= ChunkCount) break;       // 남은 조각 없음

        const uint32 Begin = static_cast<uint32>(uint64(Count) * Chunk / ChunkCount);
        const uint32 End = static_cast<uint32>(uint64(Count) * (Chunk + 1) / ChunkCount);
        (*Func)(Begin, End, Chunk);
    }
}
