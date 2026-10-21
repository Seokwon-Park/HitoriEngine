#pragma once

#include "BillboardComponent.h"

struct FParticle
{
	// 파티클 한 개의 물리·수명·SubUV 프레임 상태를 담는다.
	FVector Location;
	FVector Velocity;
	float LifeTime;

	float Age;
	float Scale;
	uint32 SubUVFrame;
	float FrameTimer;

	float Alpha;

	bool bAlive;
};



// 가장 단순한 SubUV 파티클입니다.
// Atlas 전체를 Col x Row로 나누고 CurrentFrame 하나만 변경합니다.
class UParticleSubUVComponent : public UBillboardComponent
{
	DECLARE_CLASS(UParticleSubUVComponent, UBillboardComponent)

	REFLECT_START(ClassName)
		PROPERTY(ColSize)
		PROPERTY(RowSize)
		PROPERTY(FrameRate)
	REFLECT_END()

public:
	UParticleSubUVComponent();
	virtual ~UParticleSubUVComponent() = default;
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual FBox CalcLocalBounds() const override;

	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext) override;
	// ViewportLayout가 View별 거리 정렬 입력을 만들 수 있도록 현재 파티클 배열을 읽기 전용으로 제공한다.
	const TArray<FParticle>& GetParticlesForView() const { return Particles; }
	// 프레임의 첫 View 전에 파티클 인덱스별 상수를 준비하고 네 View에서 재사용한다.
	void BeginViewSubmission();
	// 불투명 파티클의 기존 정렬 경로를 선택한다.
	bool UsesOpaqueMaterial() const;
	// ViewportLayout가 정렬한 파티클을 View별 Billboard 행렬과 거리 순서로 렌더 큐에 넣는다.
	void SubmitParticleToRenderQueue(FRenderQueue& RenderQueue, int32 ParticleIndex, const FMatrix& WorldMatrix, float CameraDistanceSquared);

	void SetSubUVSize(uint32 Cols, uint32 Rows);
	void SetFrameRate(float InFrameRate);

	void SetParticles(int32 InParticleCount = 10);

private:
	void RespawnParticle(FParticle& Particle);

	// Todo: Move to util class
	float GetRandomNumberBetween(float start, float end) const;
	float Lerp(float first, float second, float alpha) const;
	float GedSmoothStepedRatio(float start, float end, float value) const;

private:
	static const float MAX_NORMALIZED_VALUE;

	// 방출 튜닝 값
	int32 ParticleCount = 0;

	float MinLifeTime = 6.0f;           // 수명 최소값(초)
	float MaxLifeTime = 8.0f;          // 수명 최대값(초)

	float MinRiseSpeed = 0.7f;          // 상승 속도 최소값
	float MaxRiseSpeed = 0.9f;          // 상승 속도 최대값

	float StartScale = 1.2f;            // 생성 시 크기
	float EndScale = 1.5f;              // 수명이 끝날 때 크기

	float FadeStart = 0.8f;             // 수명 비율이 이 값을 넘으면 페이드아웃 시작 (0~1)

	TArray<FSubUVConstants> Constants;

	uint32 ColSize;
	uint32 RowSize;
	float FrameRate;

	// Todo: Reserve particles
	TArray<FParticle> Particles;
};
