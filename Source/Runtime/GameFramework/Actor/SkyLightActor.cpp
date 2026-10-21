#include "EnginePCH.h"
#include "SkyLightActor.h"

ASkyLightActor::ASkyLightActor()
{
    SkyLightComponent = CreateDefaultSubobject<USkyLightComponent>("USkyLightComponent");
	SetupLightRoot(SkyLightComponent, "Assets/Icons/Sky.png");
}