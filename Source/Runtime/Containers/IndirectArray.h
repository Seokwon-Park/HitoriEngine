#pragma once

#include "Array.h"

template <typename T>
class TIndirectArray
{
public:
	using ElementType = T;
	using InternalArrayType = TArray<void*>;
	//using AllocatorType = Allocator;

	//default ctor
	[[nodiscard]] TIndirectArray() = default;
	[[nodiscard]] TIndirectArray(TIndirectArray&&) = default;

	//copy
	[[nodiscard]] TIndirectArray(const TIndirectArray& Other)
	{
		for (auto& Item : Other)
		{
			Add(new T(Item));
		}
	}

	TIndirectArray& operator=(const TIndirectArray& Other)
	{
		if (&Other != this)
		{
			Empty(Other.Num());
			for (auto& Item : Other)
			{
				Add(new T(Item));
			}
		}

		return *this;
	}

	TIndirectArray& operator=(TIndirectArray&& Other)
	{
		if (&Other != this)
		{
			Empty();
			Array = std::move(Other.Array);
		}

		return *this;
	}

	//Destructor
	~TIndirectArray()
	{
		Empty();
	}

	[[nodiscard]] bool IsEmpty() const
	{
		return Array.IsEmpty();
	}

	[[nodiscard]] int32 Num() const
	{
		return Array.Num();
	}

	[[nodiscard]] T** GetData()
	{
		return (T**)Array.GetData();
	}

	[[nodiscard]] const T** GetData() const
	{
		return (const T**)Array.GetData();
	}

	[[nodiscard]] static constexpr uint32 GetTypeSize()
	{
		return sizeof(T*);
	}

	[[nodiscard]] T& operator[](int32 Index)
	{
		return *(T*)Array[Index];
	}

	[[nodiscard]] const T& operator[](int32 Index) const
	{
		return *(T*)Array[Index];
	}

	[[nodiscard]] ElementType& Last(int32 IndexFromTheEnd = 0)
	{
		return *(T*)Array.Last(IndexFromTheEnd);
	}

	[[nodiscard]] const ElementType& Last(int32 IndexFromTheEnd = 0) const
	{
		return *(T*)Array.Last(IndexFromTheEnd);
	}

	void Reset(int32 NewSize = 0)
	{
		DestructAndFreeItems();
		Array.Reset(NewSize);
	}

	void RemoveAt(int32 Index)
	{
		assert(Index >= 0);
		assert(Index < Array.Num());
		T** Element = GetData() + Index;
		delete* Element;
		Array.RemoveAt(Index);
	}

	void RemoveAt(int32 Index, int32 Count)
	{
		assert(Index >= 0);
		assert(Index <= Array.Num());
		assert(Index + Count <= Array.Num());
		T** Element = GetData() + Index;
		for (int32 ElementId = Count; ElementId; --ElementId)
		{
			delete* Element;
			++Element;
		}
		Array.RemoveAt(Index, Count);
	}

	void RemoveAtSwap(int32 Index)
	{
		assert(Index >= 0);
		assert(Index < Array.Num());
		T** Element = GetData() + Index;
		delete* Element;
		Array.RemoveAtSwap(Index);
	}

	void Empty(int32 Slack = 0)
	{
		DestructAndFreeItems();
		Array.Empty(Slack);
	}

	int32 Add(T* Item)
	{
		return Array.Add(Item);
	}

	void Insert(T* Item, int32 Index)
	{
		Array.Insert(Item, Index);
	}

	void Reserve(int32 Number)
	{
		Array.Reserve(Number);
	}

	bool IsValidIndex(int32 Index) const
	{
		return Array.IsValidIndex(Index);
	}

	[[nodiscard]] SIZE_T GetAllocatedSize() const
	{
		return Array.Max() * sizeof(T*) + Array.Num() * sizeof(T);
	}

	using TIterator = TIndexedContainerIterator<TIndirectArray, ElementType, int32>;
	using TConstIterator = TIndexedContainerIterator<const TIndirectArray, const ElementType, int32>;

	[[nodiscard]] TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	[[nodiscard]] TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	[[nodiscard]] TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType> begin() { return TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType>(Array.begin()); }
	[[nodiscard]] TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType> begin() const { return TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType>(Array.begin()); }
	[[nodiscard]] TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType> end() { return TDereferencingIterator<ElementType, typename InternalArrayType::RangedForIteratorType>(Array.end()); }
	[[nodiscard]] TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType> end() const { return TDereferencingIterator<const ElementType, typename InternalArrayType::RangedForConstIteratorType>(Array.end()); }


private:
	void DestructAndFreeItems()
	{
		T** Element = GetData();
		for (int32 Index = Array.Num(); Index; --Index)
		{
			delete* Element;
			++Element;
		}
	}

	InternalArrayType Array;
};