
#pragma once

#include <cassert>
#include <vector>

#include "Core/Types.h"

template <typename ContainerType, typename ElementType, typename SizeType>
class TIndexedContainerIterator
{
	//using ContainerAllocatorType = typename ContainerType::AllocatorType;
public:
	[[nodiscard]] TIndexedContainerIterator(ContainerType& InContainer, SizeType StartIndex = 0)
		: Container(InContainer)
		, Index(StartIndex)
	{
	}

	TIndexedContainerIterator& operator++()
	{
		++Index;
		return *this;
	}

	TIndexedContainerIterator operator++(int)
	{
		TIndexedContainerIterator Tmp(*this);
		++Index;
		return Tmp;
	}

	TIndexedContainerIterator& operator--()
	{
		--Index;
		return *this;
	}

	TIndexedContainerIterator operator--(int)
	{
		TIndexedContainerIterator Tmp(*this);
		--Index;
		return Tmp;
	}

	TIndexedContainerIterator& operator+=(SizeType Offset)
	{
		Index += Offset;
		return *this;
	}

	[[nodiscard]] TIndexedContainerIterator operator+(SizeType Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp += Offset;
	}

	TIndexedContainerIterator& operator-=(SizeType Offset)
	{
		return *this += -Offset;
	}

	[[nodiscard]] TIndexedContainerIterator operator-(SizeType Offset) const
	{
		TIndexedContainerIterator Tmp(*this);
		return Tmp -= Offset;
	}

	[[nodiscard]] ElementType& operator* () const
	{
		return Container[Index];
	}

	[[nodiscard]] ElementType* operator->() const
	{
		return &Container[Index];
	}

	[[nodiscard]] explicit operator bool() const
	{
		return Container.IsValidIndex(Index);
	}

	[[nodiscard]] SizeType GetIndex() const
	{
		return Index;
	}

	void Reset()
	{
		Index = 0;
	}

	void SetToEnd()
	{
		Index = Container.Num();
	}

	void RemoveCurrent()
	{
		Container.RemoveAt(Index);
		Index--;
	}

	void RemoveCurrentSwap()
	{
		Container.RemoveAtSwap(Index);
		Index--;
	}

	[[nodiscard]] bool operator==(const TIndexedContainerIterator& Rhs) const
	{
		return &Container == &Rhs.Container && Index == Rhs.Index;
	}

	[[nodiscard]] bool operator!=(const TIndexedContainerIterator& Rhs) const
	{
		return &Container != &Rhs.Container || Index != Rhs.Index;
	}

private:
	ContainerType& Container;
	SizeType      Index;
};

template <typename ContainerType, typename ElementType, typename SizeType>
[[nodiscard]] TIndexedContainerIterator<ContainerType, ElementType, SizeType> operator+(SizeType Offset, TIndexedContainerIterator<ContainerType, ElementType, SizeType> RHS)
{
	return RHS + Offset;
}


template <typename ElementType, typename IteratorType>
struct TDereferencingIterator
{
	[[nodiscard]] explicit TDereferencingIterator(IteratorType InIter)
		: Iter(InIter)
	{
	}

	[[nodiscard]] ElementType& operator*() const
	{
		return *(ElementType*)*Iter;
	}

	TDereferencingIterator& operator++()
	{
		++Iter;
		return *this;
	}

	[[nodiscard]] bool operator!=(const TDereferencingIterator& Rhs) const
	{
		return Iter != Rhs.Iter;
	}

private:
	IteratorType Iter;
};

template<typename T>
class TArray
{
public:
	using RangedForIteratorType = typename std::vector<T>::iterator;
	using RangedForConstIteratorType = typename std::vector<T>::const_iterator;

	using ElementType = T;
	using SizeType = uint32;
	TArray() = default;
	~TArray() = default;

	TArray(const TArray&) = default;
	TArray& operator=(const TArray&) = default;

	TArray(TArray&&) noexcept = default;
	TArray& operator=(TArray&&) noexcept = default;

	TArray(std::initializer_list<T> initList);

	T& operator[](uint32 index);

	const T& operator[](uint32 index) const;
	TArray<T>& operator=(std::initializer_list<T> initList);

	std::vector<T>::iterator begin();
	std::vector<T>::iterator end();

	std::vector<T>::const_iterator begin() const;
	std::vector<T>::const_iterator end() const;

	std::vector<T>::reverse_iterator rbegin();
	std::vector<T>::reverse_iterator rend();

	std::vector<T>::const_reverse_iterator rbegin() const;
	std::vector<T>::const_reverse_iterator rend() const;

	// Todo: Delete, Memory leak if use pointer type
	void Init(const T& data, uint32 count);

	T* GetData();
	const T* GetData() const;
	void SetNum(int32 NewNum, bool bAllowShrinking = true);

	uint32 Add(const T& data);
	uint32 Add(T&& data);

	void Push(const ElementType& Item)
	{
		Add(Item);
	}
	void Push(ElementType&& Item)
	{
		Add(std::move(Item));
	}

	template<typename... Args>
	uint32 Emplace(Args&&... args);
	uint32 Insert(const T& data, uint32 index);
	void Append(const TArray<T>& other);

	void Reserve(uint32 Number);

	bool Find(const ElementType& Item, int32& Index) const
	{
		Index = Find(Item);
		return Index != -1;
	};

	int32 Find(const ElementType& Item) const
	{
		auto It = std::find(mDatas.begin(), mDatas.end(), Item);
		if (It == mDatas.end())
		{
			return -1;
		}
		return static_cast<int32>(It - mDatas.begin());
	}

	int32 Num() const;
	int32 Max() const;

	void Empty(SizeType Slack = 0)
	{
		std::vector<T>().swap(mDatas);
		mDatas.reserve(static_cast<size_t>(Slack));
	}

	bool IsEmpty() const;

	void Reset(SizeType NewSize = 0);

	void RemoveAt(uint32 Index)
	{
		assert(Index < mDatas.size());
		mDatas.erase(mDatas.begin() + Index);
	}
	void RemoveAt(uint32 index, int32 count);
	void RemoveAtSwap(uint32 index);
	void RemoveLast();
	void Pop() { RemoveLast(); }

	size_t size() const { return mDatas.size(); }

	[[nodiscard]] ElementType& Last(SizeType IndexFromTheEnd = 0)
	{
		//RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		assert(mDatas.size() - IndexFromTheEnd - 1 >= 0);
		return GetData()[mDatas.size() - IndexFromTheEnd - 1];
	}
	[[nodiscard]] const ElementType& Last(SizeType IndexFromTheEnd = 0) const
	{
		return const_cast<TArray*>(this)->Last(IndexFromTheEnd);
	}

	ElementType& Top() { return Last(); }

	T& Front();
	const T& Front() const;

	[[nodiscard]] bool IsValidIndex(int32 Index) const
	{
		return Index >= 0 && Index < mDatas.size();
	}

	[[nodiscard]] T& AddDefaulted_GetRef()
	{
		return mDatas.emplace_back();
	}


private:
	std::vector<T> mDatas;
	SizeType ArrayNum; // Size
	SizeType ArrayMax; // Capacity
};

template<typename T>
TArray<T>::TArray(std::initializer_list<T> initList)
	: mDatas(initList)
{
}

template<typename T>
inline T& TArray<T>::operator[](uint32 index)
{
	assert(index < mDatas.size());

	return mDatas[index];
}

template<typename T>
inline const T& TArray<T>::operator[](uint32 index) const
{
	assert(index < mDatas.size());

	return mDatas[index];
}

template<typename T>
TArray<T>& TArray<T>::operator=(std::initializer_list<T> initList)
{
	mDatas = initList;

	return *this;
}

template<typename T>
inline std::vector<T>::iterator TArray<T>::begin()
{
	return mDatas.begin();
}

template<typename T>
inline std::vector<T>::iterator TArray<T>::end()
{
	return mDatas.end();
}

template<typename T>
inline std::vector<T>::const_iterator TArray<T>::begin() const
{
	return mDatas.cbegin();
}

template<typename T>
inline std::vector<T>::const_iterator TArray<T>::end() const
{
	return mDatas.cend();
}

template<typename T>
inline std::vector<T>::reverse_iterator TArray<T>::rbegin()
{
	return mDatas.rbegin();
}

template<typename T>
inline std::vector<T>::reverse_iterator TArray<T>::rend()
{
	return mDatas.rend();
}

template<typename T>
inline std::vector<T>::const_reverse_iterator TArray<T>::rbegin() const
{
	return mDatas.crbegin();
}

template<typename T>
inline std::vector<T>::const_reverse_iterator TArray<T>::rend() const
{
	return mDatas.crend();
}


// Todo: Need to fix code
template<typename T>
inline void TArray<T>::Init(const T& data, uint32 count)
{
	// Todo: Check memory leak
	// Memory leak if use pointer type on data
	mDatas.assign(count, data);
}

template<typename T>
inline T* TArray<T>::GetData()
{
	return mDatas.data();
}

template<typename T>
inline const T* TArray<T>::GetData() const
{
	return mDatas.data();
}

template<typename T>
inline uint32 TArray<T>::Add(const T& data)
{
	mDatas.push_back(data);

	return static_cast<uint32>(mDatas.size()) - 1;
}

template<typename T>
inline uint32 TArray<T>::Add(T&& data)
{
	mDatas.push_back(std::move(data));

	return static_cast<uint32>(mDatas.size()) - 1;
}

template <typename T>
template <typename ... Args>
uint32 TArray<T>::Emplace(Args&&... args)
{
	mDatas.emplace_back(std::forward<Args>(args)...);
	return static_cast<uint32>(mDatas.size()) - 1;
}

template<typename T>
inline uint32 TArray<T>::Insert(const T& data, uint32 index)
{
	mDatas.insert(mDatas.begin() + index, data);

	return index;
}

template<typename T>
inline void TArray<T>::Append(const TArray<T>& other)
{
	mDatas.insert(mDatas.end(), other.begin(), other.end());
}

template<typename T>
inline int32 TArray<T>::Num() const
{
	return static_cast<int32>(mDatas.size());
}

template<typename T>
inline void TArray<T>::Reserve(uint32 capacity)
{
	mDatas.reserve(capacity);
}

// 원소 개수를 NewNum으로 맞춘다. 늘어난 원소는 기본 생성자로 초기화된다.
// bAllowShrinking이면 줄어들 때 남는 메모리도 반환한다.
template<typename T>
inline void TArray<T>::SetNum(int32 NewNum, bool bAllowShrinking)
{
	assert(NewNum >= 0);

	const size_t OldNum = mDatas.size();
	mDatas.resize(static_cast<size_t>(NewNum));

	if (bAllowShrinking && static_cast<size_t>(NewNum) < OldNum)
	{
		mDatas.shrink_to_fit();
	}
}

template<typename T>
inline int32 TArray<T>::Max() const
{
	return static_cast<uint32>(mDatas.capacity());
}

template<typename T>
inline bool TArray<T>::IsEmpty() const
{
	return mDatas.empty();
}

template<typename T>
inline void TArray<T>::Reset(SizeType NewSize)
{
	mDatas.clear();                              // 용량 유지
	mDatas.reserve(static_cast<size_t>(NewSize)); // 모자랄 때만 재할당
}


template<typename T>
inline void TArray<T>::RemoveAt(uint32 index, int32 count)
{
	assert(mDatas.empty() == false);
	assert(index < mDatas.size());
	assert((index + count) <= mDatas.size());

	auto removeBeginIter = mDatas.begin() + index;
	mDatas.erase(removeBeginIter, removeBeginIter + count);
}

template<typename T>
inline void TArray<T>::RemoveAtSwap(uint32 index)
{
	assert(mDatas.empty() == false);
	assert(index < mDatas.size());

	T moveData = mDatas.back();
	mDatas[index] = moveData;

	RemoveLast();
}

template<typename T>
inline void TArray<T>::RemoveLast()
{
	assert(mDatas.empty() == false);

	mDatas.erase(mDatas.begin() + mDatas.size() - 1);

}

template<typename T>
inline T& TArray<T>::Front()
{
	assert(!mDatas.empty());
	return mDatas.front();
}

template<typename T>
inline const T& TArray<T>::Front() const
{
	assert(!mDatas.empty());
	return mDatas.front();
}
