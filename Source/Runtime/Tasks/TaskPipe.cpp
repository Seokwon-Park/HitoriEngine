#include "EnginePCH.h"
#include "TaskPipe.h"

namespace Tasks
{
    FPipe::FPipe(std::string InDebugName)
        : DebugName(std::move(InDebugName))
    {
    }

    void FPipe::WaitUntilEmpty()
    {
        FTask TaskToWait;
        {
            std::lock_guard<std::mutex> Lock(PipeMutex);
            TaskToWait = LastTask;
        }

        if (TaskToWait.IsValid())
        {
            TaskToWait.Wait();
        }
    }
}
