#pragma once

#include "TaskTypes.h"

namespace Tasks
{
    // 원시 태스크 실행 함수 원형
    using FLowLevelTaskFunction = void (*)(void* UserData);

    // 저수준 태스크 단위
    struct FLowLevelTask
    {
        FLowLevelTaskFunction Function = nullptr;
        void* UserData = nullptr;
        ETaskPriority Priority = ETaskPriority::Normal;

        void Execute() const
        {
            if (Function)
            {
                Function(UserData);
            }
        }
    };
}
