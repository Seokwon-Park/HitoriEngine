#pragma once

#include "GameFramework/Actor.h"
#include "Components/TextRenderComponent.h"

class ATextRenderActor : public AActor
{
	DECLARE_CLASS(ATextRenderActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ATextRenderActor();
	virtual ~ATextRenderActor();

	inline UTextRenderComponent* GetTextRenderComponent() const { return TextRenderComponent; }

private:
	UTextRenderComponent* TextRenderComponent;
};
