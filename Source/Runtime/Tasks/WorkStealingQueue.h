#pragma once

#include "LowLevelTask.h"
#include <atomic>
#include <array>
#include <cstdint>

namespace Tasks
{
    // Single-owner work-stealing queue. Atomic slots protect delayed thieves during ring reuse.
    class FWorkStealingQueue
    {
    public:
        static constexpr int64_t Capacity = 4096;
        static constexpr int64_t Mask = Capacity - 1;

        FWorkStealingQueue()
            : Top(0)
            , Bottom(0)
        {
        }

        // 소유 스레드 일감 등록
        bool Push(const FLowLevelTask& Task)
        {
            const int64_t CurrentBottom = Bottom.load(std::memory_order_relaxed);
            const int64_t CurrentTop = Top.load(std::memory_order_acquire);

            if (CurrentBottom - CurrentTop >= Capacity)
            {
                return false;
            }

            Buffer[CurrentBottom & Mask].store(Task, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_release);
            Bottom.store(CurrentBottom + 1, std::memory_order_relaxed);
            return true;
        }

        // 소유 스레드 일감 인출
        bool Pop(FLowLevelTask& OutTask)
        {
            int64_t CurrentBottom = Bottom.load(std::memory_order_relaxed);
            CurrentBottom = CurrentBottom - 1;
            Bottom.store(CurrentBottom, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);

            const int64_t CurrentTop = Top.load(std::memory_order_relaxed);

            if (CurrentTop <= CurrentBottom)
            {
                if (CurrentTop == CurrentBottom)
                {
                    int64_t ExpectedTop = CurrentTop;
                    if (!Top.compare_exchange_strong(ExpectedTop, CurrentTop + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
                    {
                        Bottom.store(CurrentBottom + 1, std::memory_order_relaxed);
                        return false;
                    }

                    OutTask = Buffer[CurrentBottom & Mask].load(std::memory_order_relaxed);
                    Bottom.store(CurrentBottom + 1, std::memory_order_relaxed);
                    return true;
                }

                OutTask = Buffer[CurrentBottom & Mask].load(std::memory_order_relaxed);
                return true;
            }

            Bottom.store(CurrentBottom + 1, std::memory_order_relaxed);
            return false;
        }

        // 타 스레드 일감 강탈
        bool Steal(FLowLevelTask& OutTask)
        {
            while (true)
            {
                const int64_t CurrentTop = Top.load(std::memory_order_acquire);
                std::atomic_thread_fence(std::memory_order_seq_cst);
                const int64_t CurrentBottom = Bottom.load(std::memory_order_acquire);

                if (CurrentTop >= CurrentBottom)
                {
                    return false;
                }

                OutTask = Buffer[CurrentTop & Mask].load(std::memory_order_relaxed);

                int64_t ExpectedTop = CurrentTop;
                if (Top.compare_exchange_strong(ExpectedTop, CurrentTop + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
                {
                    return true;
                }
            }
        }

        int64_t Num() const
        {
            const int64_t CurrentBottom = Bottom.load(std::memory_order_relaxed);
            const int64_t CurrentTop = Top.load(std::memory_order_relaxed);
            const int64_t Count = CurrentBottom - CurrentTop;
            return Count > 0 ? Count : 0;
        }

    private:
        std::atomic<int64_t> Top; // 가장 오래된 일감 위치
        std::atomic<int64_t> Bottom; // 가장 최근 일감 및 빈 슬롯
        std::array<std::atomic<FLowLevelTask>, Capacity> Buffer{};
    };
}
