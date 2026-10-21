#pragma once

#include "Engine.h"

class UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine, UEngine)
public:
	FEngineConfig GetConfig() const override;

	virtual bool Init() override;
	virtual void Tick(float DeltaTime)override;
private:
	
};