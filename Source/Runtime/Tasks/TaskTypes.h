#pragma once

#include <cstdint>

namespace Tasks
{
    // 태스크 우선순위
    enum class ETaskPriority : uint8_t
    {
        High,
        Normal,
        Background
    };

    // 태스크 상태
    enum class ETaskState : uint8_t
    {
        Constructed,
        Pending,
        Queued,
        Executing,
        Completed
    };
}
