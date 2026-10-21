#include "EnginePCH.h"
#include "TextRenderActor.h"

ATextRenderActor::ATextRenderActor()
{
	TextRenderComponent = CreateDefaultSubobject<UTextRenderComponent>("TextRenderComponent");
	TextRenderComponent->SetupAttachment(GetRootComponent());
}

ATextRenderActor::~ATextRenderActor()
{
}
