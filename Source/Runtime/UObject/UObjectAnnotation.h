#pragma once

#include <xmemory>
#include "Containers/Map.h"


template<typename TAnnotation>
class FUObjectAnnotationTemp
{
public:
	FUObjectAnnotationTemp()
	{
		assert(AnnotationCacheValue.IsDefault());
	}

	virtual ~FUObjectAnnotationTemp()
	{
		RemoveAllAnnotations();
	}

private:
	template<typename T>
	void AddAnnotationInternal(const UObject* Object, T&& Annotation)
	{
		TAnnotation LocalAnnotation = std::forward<T>(Annotation);
		if (LocalAnnotation.IsDefault())
		{
			RemoveAnnotation(Object); // adding the default annotation is the same as removing an annotation
		}
		else
		{
			AnnotationMap.Add(Object, LocalAnnotation);
			SetAnnotationCacheKeyAndValue(Object, std::move(LocalAnnotation));
		}
	}

public:
	void AddAnnotation(const UObject* Object, TAnnotation&& Annotation)
	{
		AddAnnotationInternal(Object, std::move(Annotation));
	}

	void AddAnnotation(const UObject* Object, const TAnnotation& Annotation)
	{
		AddAnnotationInternal(Object, Annotation);
	}

	TAnnotation GetAndRemoveAnnotation(const UObject* Object)
	{
		TAnnotation Result;
		SetAnnotationCacheKeyAndValue(Object, TAnnotation());
		AnnotationMap.RemoveAndCopyValue(Object, Result);

		return Result;
	}
	
	void RemoveAnnotation(const UObject* Object)
	{
		SetAnnotationCacheKeyAndValue(Object, TAnnotation());
		AnnotationMap.Remove(Object);
	}

	void RemoveAllAnnotations()
	{
		SetAnnotationCacheKeyAndValue(nullptr, TAnnotation());
		AnnotationMap.Empty();
	}

	inline TAnnotation GetAnnotation(const UObject* Object)
	{
		if (Object == AnnotationCacheKey)
		{
			return AnnotationCacheValue;
		}

		TAnnotation* Entry = AnnotationMap.Find(Object);
		SetAnnotationCacheKeyAndValue(Object, Entry ? *Entry : TAnnotation());

		return Entry ? *Entry : TAnnotation();
	}

	const TMap<const UObject*, TAnnotation>& GetAnnotationMap() const
	{
		return AnnotationMap;
	}

	void Reserve(int32 ExpectedNumElements)
	{
		AnnotationMap.Empty(ExpectedNumElements);
	}
private:
	void SetAnnotationCacheKeyAndValue(const UObject* Key, const TAnnotation& Value)
	{
		AnnotationCacheKey = Key;
		AnnotationCacheValue = Value;
	}

	void SetAnnotationCacheKeyAndValue(const UObject* Key, TAnnotation&& Value)
	{
		AnnotationCacheKey = Key;
		AnnotationCacheValue = std::move(Value);
	}

protected:
	TMap<const UObject*, TAnnotation> AnnotationMap;

	//같은 객체를 연달아 조회하는 패턴이 흔하기 때문에 캐싱해둔다.
	const UObject* AnnotationCacheKey = nullptr;

	TAnnotation AnnotationCacheValue;
};