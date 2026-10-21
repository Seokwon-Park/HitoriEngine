#pragma once

#include "Core/Types.h"
#include "Input/InputSystem.h"

class FViewport;
class UWorld;

class FViewportClient
{
public:
	virtual ~FViewportClient() = default;
	virtual UWorld* GetWorld() const { return nullptr; }
	virtual void MouseMove(FViewport*, int32, int32) {}
	virtual bool InputKey(FViewport*, int32, bool) { return false; }
	virtual bool InputAxis(FViewport*, int32, EGameInputAxis, float, float) { return false; }
	virtual void ReceivedFocus() {}
	virtual void LostFocus() {}
	virtual void Resize(FViewport*, uint32, uint32) {}
};
