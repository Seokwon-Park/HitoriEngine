#pragma once

#include "TaskBase.h"
#include <memory>

namespace Tasks
{
    // 값 타입 경량 태스크 핸들
    class FTask
    {
    public:
        FTask() = default;

        explicit FTask(FTaskBase* InImpl)
            : Impl(InImpl)
        {
            if (Impl)
            {
                Impl->AddRef();
            }
        }

        FTask(const FTask& Other)
            : Impl(Other.Impl)
        {
            if (Impl)
            {
                Impl->AddRef();
            }
        }

        FTask(FTask&& Other) noexcept
            : Impl(Other.Impl)
        {
            Other.Impl = nullptr;
        }

        FTask& operator=(const FTask& Other)
        {
            if (this != &Other)
            {
                if (Other.Impl)
                {
                    Other.Impl->AddRef();
                }
                if (Impl)
                {
                    Impl->Release();
                }
                Impl = Other.Impl;
            }
            return *this;
        }

        FTask& operator=(FTask&& Other) noexcept
        {
            if (this != &Other)
            {
                if (Impl)
                {
                    Impl->Release();
                }
                Impl = Other.Impl;
                Other.Impl = nullptr;
            }
            return *this;
        }

        ~FTask()
        {
            if (Impl)
            {
                Impl->Release();
                Impl = nullptr;
            }
        }

        bool IsValid() const { return Impl != nullptr; }

        bool IsCompleted() const
        {
            return Impl ? Impl->IsCompleted() : true;
        }

        void Wait() const
        {
            if (Impl)
            {
                Impl->Wait();
            }
        }

        FTaskBase* GetImpl() const { return Impl; }

    private:
        FTaskBase* Impl = nullptr;
    };

    // 결과값을 전달하는 템플릿 태스크 핸들
    template<typename ResultType>
    class TTask
    {
    public:
        TTask() = default;

        TTask(FTaskBase* InImpl, std::shared_ptr<ResultType> InResultStorage)
            : TaskHandle(InImpl)
            , ResultStorage(std::move(InResultStorage))
        {
        }

        bool IsValid() const { return TaskHandle.IsValid(); }
        bool IsCompleted() const { return TaskHandle.IsCompleted(); }
        void Wait() const { TaskHandle.Wait(); }

        const ResultType& GetResult() const
        {
            Wait();
            return *ResultStorage;
        }

        FTask AsTask() const { return TaskHandle; }

    private:
        FTask TaskHandle;
        std::shared_ptr<ResultType> ResultStorage;
    };
}
