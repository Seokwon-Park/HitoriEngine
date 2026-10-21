#include "EnginePCH.h"
#include "TaskBase.h"
#include "TaskScheduler.h"

namespace Tasks
{
    FTaskBase::FTaskBase(FExecutableFunction InExecutable, const ETaskPriority InPriority)
        : Priority(InPriority)
        , Executable(std::move(InExecutable))
    {
        LowLevelTask.Function = &FTaskBase::LowLevelTaskCallback;
        LowLevelTask.UserData = this;
        LowLevelTask.Priority = InPriority;
    }

    void FTaskBase::AddRef()
    {
        RefCount.fetch_add(1, std::memory_order_relaxed);
    }

    void FTaskBase::Release()
    {
        if (RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    void FTaskBase::LowLevelTaskCallback(void* UserData)
    {
        if (FTaskBase* Task = static_cast<FTaskBase*>(UserData))
        {
            Task->Execute();
            Task->Release();
        }
    }

    void FTaskBase::AddPrerequisite(FTaskBase* InPrerequisite)
    {
        if (!InPrerequisite)
        {
            return;
        }

        // 이미 완료된 선행 작업이면 무시
        if (InPrerequisite->IsCompleted())
        {
            return;
        }

        PrerequisitesCount.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> Lock(InPrerequisite->SubscribersMutex);
        if (InPrerequisite->IsCompleted())
        {
            // 등록 도중 완료된 경우 즉시 복원
            PrerequisitesCount.fetch_sub(1, std::memory_order_relaxed);
        }
        else
        {
            // A prerequisite must keep its dependent alive even if its handle is discarded.
            AddRef();
            try { InPrerequisite->Subscribers.push_back(this); }
            catch (...) { PrerequisitesCount.fetch_sub(1, std::memory_order_relaxed); Release(); throw; }
        }
    }

    void FTaskBase::OnPrerequisiteCompleted()
    {
        // 모든 선행 작업 완료 시 스케줄러에 일감 등록
        if (PrerequisitesCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            AddRef();
            FTaskScheduler::Get().Schedule(LowLevelTask);
        }
    }

    void FTaskBase::Execute()
    {
        try
        {
            if (Executable) Executable();
        }
        catch (...)
        {
            Exception = std::current_exception();
        }
        OnCompleted();
    }

    void FTaskBase::OnCompleted()
    {
        std::vector<FTaskBase*> SubscribersToNotify;
        {
            std::lock_guard<std::mutex> Lock(SubscribersMutex);
            bIsCompleted.store(true, std::memory_order_release);
            SubscribersToNotify.swap(Subscribers);
        }

        // 후속 구독 태스크에 완료 통보
        for (FTaskBase* Subscriber : SubscribersToNotify)
        {
            if (Subscriber)
            {
                Subscriber->OnPrerequisiteCompleted();
                Subscriber->Release();
            }
        }
    }

    void FTaskBase::Wait()
    {
        if (!IsCompleted())
            FTaskScheduler::Get().HelpSteal([this] { return IsCompleted(); });
        // Completion's acquire/release pair also publishes the captured exception.
        if (Exception) std::rethrow_exception(Exception);
    }
}
