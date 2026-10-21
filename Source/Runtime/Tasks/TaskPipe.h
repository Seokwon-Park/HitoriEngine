#pragma once

#include "Task.h"
#include <mutex>
#include <string>

namespace Tasks
{
    // 순차 실행을 보장하는 태스크 파이프
    class FPipe
    {
    public:
        FPipe(std::string InDebugName = "Pipe");
        ~FPipe() = default;

        // 파이프에 순차 실행할 작업 등록
        template<typename FunctorType>
        FTask Launch(FunctorType&& Functor, ETaskPriority Priority = ETaskPriority::Normal)
        {
            FTaskBase* NewTaskBase = new FTaskBase(std::forward<FunctorType>(Functor), Priority);
            FTask NewTask(NewTaskBase);
            {
                std::lock_guard<std::mutex> Lock(PipeMutex);
                if (LastTask.IsValid()) NewTaskBase->AddPrerequisite(LastTask.GetImpl());
                LastTask = NewTask;
            }
            // Scheduling can execute inline; do not hold PipeMutex while running user code.
            NewTaskBase->OnPrerequisiteCompleted();

            return NewTask;
        }

        // 파이프의 모든 작업 완료 대기
        void WaitUntilEmpty();

    private:
        std::string DebugName;
        std::mutex PipeMutex;
        FTask LastTask;
    };
}
