#pragma once
#include <unordered_map>
#include <vector>

class AActor;

class FPathTracker
{
public:
    void SetPathRenderingEnabled(bool bEnable);
    void ClearPath();

    bool IsEnabled() const;

    void Tick(const TArray<AActor*>& ActorList, float DeltaTime);

    void OnRender(class FLineBatcher* LineBatcher);

    void OnObjectDestroyed(AActor* InActor);

    void SetPlaybackEnabled(bool bEnable);

private:
    bool bIsPathRenderingEnabled = false;
    float RecordInterval = 0.01f;
    float TimeSinceLastRecord = 0.0f;

    bool bIsPlayingBack = false;
    float PlaybackTime = 0.0f;

    std::unordered_map<class AActor*, TArray<FVector>> ObjectPaths;
};