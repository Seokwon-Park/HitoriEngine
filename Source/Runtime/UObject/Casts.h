#pragma once

#include "Object.h"
#include <cassert>	

template <typename T>
T* Cast(UObject* Object)
{
	if (!Object || !Object->IsA<T>())
	{
		return nullptr;
	}
	return static_cast<T*>(Object);
}

template <typename T>
const T* Cast(const UObject* Object)
{
	if (!Object || !Object->IsA<T>())
	{
		return nullptr;
	}
	return static_cast<const T*>(Object);
}

template <typename T>
T* CastChecked(UObject* Object)
{
	T* Result = Cast<T>(Object);
	assert(Result);
	return Result;
}