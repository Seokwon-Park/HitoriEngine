#pragma once

#include "UObject/Object.h"
#include "UObject/UObjectAnnotation.h"

struct FDuplicatedObject
{
	bool bIsDefault;
	UObject* DuplicatedObject;

	FDuplicatedObject() : bIsDefault(true) {};
	FDuplicatedObject(UObject* InDuplicatedObject)
		: bIsDefault(!InDuplicatedObject)
		, DuplicatedObject(InDuplicatedObject != INVALID_OBJECT ? InDuplicatedObject : nullptr)
	{
	}

	/**
	 * @return true if this is the default annotation and holds no information about a duplicated object
	 */
	bool IsDefault()
	{
		return bIsDefault;
	}
};

using FDuplicatedObjectAnnotation = FUObjectAnnotationTemp<FDuplicatedObject>;