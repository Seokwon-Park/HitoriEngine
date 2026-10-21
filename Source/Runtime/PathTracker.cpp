#include "EnginePCH.h"
#include "PathTracker.h"
#include "UObject/Object.h"

#include "GameFramework//Actor.h"
#include "Render/LineBatcher.h"

void FPathTracker::SetPathRenderingEnabled(bool bEnable)
{
    bIsPathRenderingEnabled = bEnable;

    if (!bIsPathRenderingEnabled)
    {
        TimeSinceLastRecord = 0.0f;
    }
}

void FPathTracker::ClearPath()
{
    ObjectPaths.clear();
}

bool FPathTracker::IsEnabled() const
{
    return bIsPathRenderingEnabled;
}

void FPathTracker::Tick(const TArray<AActor*>& ActorList, float DeltaTime)
{
    // 재생 모드
    if (bIsPlayingBack)
    {
        PlaybackTime += DeltaTime;

        float FloatIndex = PlaybackTime / RecordInterval;
        int32 IndexA = static_cast<int32>(FloatIndex);
        int32 IndexB = IndexA + 1;
        float Alpha = FloatIndex - IndexA;

         bool bAllFinished = true;
        for (auto& Pair : ObjectPaths)
        {
            AActor* Actor = Pair.first;
            const TArray<FVector>& Path = Pair.second;

            if (!Actor || Path.Num() == 0)
            {
                continue;
            }

            if (IndexA >= Path.Num() - 1)
            {
                Actor->GetRootComponent()->SetRelativeLocation(Path.Last());
                continue;
            }

            bAllFinished = false;

            FVector PosA = Path[IndexA];
            FVector PosB = Path[IndexB];
            FVector CurrentPos;
            CurrentPos.X = PosA.X + (PosB.X - PosA.X) * Alpha;
            CurrentPos.Y = PosA.Y + (PosB.Y - PosA.Y) * Alpha;
            CurrentPos.Z = PosA.Z + (PosB.Z - PosA.Z) * Alpha;

            Actor->GetRootComponent()->SetRelativeLocation(CurrentPos);
        }

        if (bAllFinished)
        {
            SetPlaybackEnabled(false);
        }

        return;
    }

    // 녹화모드
    if (!bIsPathRenderingEnabled)
    {
        return;
    }

    TimeSinceLastRecord += DeltaTime;
    if (TimeSinceLastRecord >= RecordInterval)
    {
        TimeSinceLastRecord = 0.0f;

        for (AActor* Actor : ActorList)
        {
            FVector CurrentLoc = Actor->GetActorLocation();
            TArray<FVector>& Path = ObjectPaths[Actor];
            if (Actor)
            {
                ObjectPaths[Actor].Add(Actor->GetActorLocation());
            }
        }
    }
}

void FPathTracker::OnRender(FLineBatcher* LineBatcher)
{

    for (const auto& Pair : ObjectPaths)
    {
        AActor* Actor = Pair.first;
        const TArray<FVector>& PathHistory = Pair.second;

        if (PathHistory.Num() < 2) continue;

        // 포인터 주소값을 해싱하여 고유한 정수값 생성
        size_t Hash = std::hash<void*>{}(Actor);

        // 비트 연산을 통해 0.0 ~ 1.0 사이의 RGB 값 추출
        float R = ((Hash >> 16) & 0xFF) / 255.0f;
        float G = ((Hash >> 8) & 0xFF) / 255.0f;
        float B = (Hash & 0xFF) / 255.0f;

        // 밝기 보정
        //R = 0.2f + R * 0.8f;
        //G = 0.2f + G * 0.8f;
        //B = 0.2f + B * 0.8f;

        LineBatcher->AddPath(PathHistory, { R, G, B, 1.0f });
    }
}

void FPathTracker::OnObjectDestroyed(AActor* InActor)
{
    ObjectPaths.erase(InActor);
}

void FPathTracker::SetPlaybackEnabled(bool bEnable)
{
    bIsPlayingBack = bEnable;
    if (bIsPlayingBack)
    {
        bIsPathRenderingEnabled = false;
        PlaybackTime = 0.0f;
    }
}
