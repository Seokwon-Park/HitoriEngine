#pragma once

#include "TaskTypes.h"
#include "TaskScheduler.h"
#include "TaskBase.h"
#include "Task.h"
#include "TaskPipe.h"
#include <type_traits>
#include <vector>
#include <algorithm>
#include <exception>
#include <utility>

namespace Tasks
{
    // 보이드 반환 람다 디스패치
    template<typename FunctorType>
    requires std::is_void_v<std::invoke_result_t<FunctorType>>
    FTask Launch(const char* DebugName, FunctorType&& Functor, ETaskPriority Priority = ETaskPriority::Normal)
    {
        (void)DebugName;

        FTaskBase* TaskImpl = new FTaskBase(std::forward<FunctorType>(Functor), Priority);
        FTask TaskHandle(TaskImpl);

        // 선행 조건이 없으므로 바로 등록
        TaskImpl->OnPrerequisiteCompleted();

        return TaskHandle;
    }

    // 선행 작업이 있는 보이드 반환 람다 디스패치
    template<typename FunctorType>
    requires std::is_void_v<std::invoke_result_t<FunctorType>>
    FTask Launch(const char* DebugName, FunctorType&& Functor, const FTask& Prerequisite, ETaskPriority Priority = ETaskPriority::Normal)
    {
        (void)DebugName;

        FTaskBase* TaskImpl = new FTaskBase(std::forward<FunctorType>(Functor), Priority);
        FTask TaskHandle(TaskImpl);

        if (Prerequisite.IsValid())
        {
            TaskImpl->AddPrerequisite(Prerequisite.GetImpl());
        }

        TaskImpl->OnPrerequisiteCompleted();

        return TaskHandle;
    }

    // 결과값을 반환하는 람다 디스패치
    template<typename FunctorType>
    requires (!std::is_void_v<std::invoke_result_t<FunctorType>>)
    auto Launch(const char* DebugName, FunctorType&& Functor, ETaskPriority Priority = ETaskPriority::Normal)
    {
        (void)DebugName;
        using ReturnType = std::invoke_result_t<FunctorType>;

        auto ResultStorage = std::make_shared<ReturnType>();

        FTaskBase* TaskImpl = new FTaskBase([Func = std::forward<FunctorType>(Functor), Storage = ResultStorage]() mutable
        {
            *Storage = Func();
        }, Priority);

        TTask<ReturnType> TaskHandle(TaskImpl, ResultStorage);

        TaskImpl->OnPrerequisiteCompleted();

        return TaskHandle;
    }

    // 병렬 반복문 분할 실행
    template<typename FunctorType>
    void ParallelFor(int32_t TotalCount, int32_t ChunkSize, const FunctorType& Functor, ETaskPriority Priority = ETaskPriority::Normal)
    {
        if (TotalCount <= 0)
        {
            return;
        }

        if (ChunkSize <= 0)
        {
            ChunkSize = 1;
        }

        const int32_t NumJobs = 1 + (TotalCount - 1) / ChunkSize;
        if (NumJobs <= 1)
        {
            Functor(0, TotalCount);
            return;
        }

        std::vector<FTask> TaskHandles;
        TaskHandles.reserve(NumJobs - 1);

        std::exception_ptr Exception;
        try
        {
            for (int32_t Index = 0; Index < NumJobs - 1; ++Index)
            {
                const int32_t Start = Index * ChunkSize;
                const int32_t End = Start + ChunkSize;
                TaskHandles.push_back(Launch("ParallelForTask", [Start, End, &Functor]
                {
                    Functor(Start, End);
                }, Priority));
            }
            const int32_t LastStart = (NumJobs - 1) * ChunkSize;
            Functor(LastStart, TotalCount);
        }
        catch (...) { Exception = std::current_exception(); }
        // Always finish all tasks before releasing Functor or the caller's captured data.
        for (const FTask& Handle : TaskHandles)
        {
            try { Handle.Wait(); }
            catch (...) { if (!Exception) Exception = std::current_exception(); }
        }
        if (Exception) std::rethrow_exception(Exception);
    }

    // Exact chunk IDs are needed by per-chunk render groups and occlusion histograms.
    template<typename FunctorType>
    void ParallelForChunks(uint32_t TotalCount, uint32_t ChunkCount, const FunctorType& Functor)
    {
        if (TotalCount == 0) return;
        ChunkCount = std::clamp(ChunkCount, 1u, TotalCount);
        const auto ExecuteChunks = [&](int32_t First, int32_t Last)
        {
            for (int32_t Index = First; Index < Last; ++Index)
            {
                const uint32_t Chunk = static_cast<uint32_t>(Index);
                const uint32_t Begin = static_cast<uint32_t>(uint64_t(TotalCount) * Chunk / ChunkCount);
                const uint32_t End = static_cast<uint32_t>(uint64_t(TotalCount) * (Chunk + 1) / ChunkCount);
                Functor(Begin, End, Chunk);
            }
        };
        if (!FTaskScheduler::Get().IsRunning() || FTaskScheduler::Get().GetNumWorkers() == 0)
            ExecuteChunks(0, static_cast<int32_t>(ChunkCount));
        else
            ParallelFor(static_cast<int32_t>(ChunkCount), 1, ExecuteChunks);
    }

}
