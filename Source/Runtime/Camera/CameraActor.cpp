#include "EnginePCH.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "../Components/ActorComponent.h"

// 카메라 컴포넌트를 기본 서브오브젝트로 만들어 루트에 연결한다.
ACameraActor::ACameraActor()
{
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	SetRootComponent(CameraComponent);
}
