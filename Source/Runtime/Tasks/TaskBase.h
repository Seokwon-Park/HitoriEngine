#pragma once

#include "TaskTypes.h"
#include "LowLevelTask.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <functional>
#include <exception>

namespace Tasks
{
    // 내부 방향성 비순환 그래프 태스크 본체
    class FTaskBase
    {
    public:
        using FExecutableFunction = std::function<void()>;

        FTaskBase(FExecutableFunction InExecutable = nullptr, ETaskPriority InPriority = ETaskPriority::Normal);
        ~FTaskBase() = default;

        // 참조 카운트 증가
        void AddRef();

        // 참조 카운트 감소 및 소멸
        void Release();

        // 선행 태스크 추가
        void AddPrerequisite(FTaskBase* InPrerequisite);

        // 선행 태스크 완료 시 호출
        void OnPrerequisiteCompleted();

        // 태스크 본체 실행
        void Execute();

        // 완료 처리 및 후속 구독자 알림
        void OnCompleted();

        // 태스크 완료 대기
        void Wait();

        bool IsCompleted() const
        {
            return bIsCompleted.load(std::memory_order_acquire);
        }

    private:
        static void LowLevelTaskCallback(void* UserData);

    private:
        std::atomic<uint32_t> RefCount{ 0 };
        std::atomic<uint32_t> PrerequisitesCount{ 1 };
        std::atomic<bool> bIsCompleted{ false };

        ETaskPriority Priority = ETaskPriority::Normal;
        FExecutableFunction Executable;
        std::exception_ptr Exception;
        FLowLevelTask LowLevelTask{};

        std::mutex SubscribersMutex;
        std::vector<FTaskBase*> Subscribers;
    };
}
