#pragma once

#include <cassert>
#include <unordered_map>
#include <utility>
#include <initializer_list>
#include "Core/Types.h"
#include <functional>


template <typename InKeyType, typename InValueType, typename KeyFuncs = std::hash<InKeyType>>
class TMap
{
public:
	using KeyType = InKeyType;
	using ValueType = InValueType;
	//using SetAllocatorType = SetAllocator;
	using KeyFuncsType = KeyFuncs;

	using MapType = std::unordered_map<KeyType, ValueType, KeyFuncs>;

	TMap() = default;
	TMap(std::initializer_list<std::pair<const KeyType, ValueType>> initList) : mMap(initList) {}

	~TMap() = default;

	typename MapType::iterator begin() { return mMap.begin(); }
	typename MapType::iterator end() { return mMap.end(); }

	typename MapType::const_iterator begin() const { return mMap.cbegin(); }
	typename MapType::const_iterator end() const { return mMap.cend(); }

	void Add(const KeyType& Key, const ValueType& Value)
	{
		mMap[Key] = Value;
	}

	int32 Remove(const KeyType& Key)
	{
		return static_cast<int32>(mMap.erase(Key));
	}

	uint32 Num() const
	{
		return static_cast<uint32>(mMap.size());
	}

	void Reset()
	{
		mMap.clear();
	}

	void Empty(int32 ExpectedNumElements = 0)
	{
		mMap.clear();
		mMap.reserve(static_cast<size_t>(ExpectedNumElements));
	}

	ValueType* Find(const KeyType& Key)
	{
		auto Iter = mMap.find(Key);
		return Iter == mMap.end() ? nullptr : &Iter->second;
	}

	const ValueType* Find(const KeyType& Key) const
	{
		auto Iter = mMap.find(Key);
		return Iter == mMap.end() ? nullptr : &Iter->second;
	}

	// 키가 없으면 ValueType의 기본값을 복사해 돌려준다 (포인터 값이면 nullptr)
	ValueType FindRef(const KeyType& Key) const
	{
		const ValueType* Found = Find(Key);
		return Found ? *Found : ValueType();
	}

	bool Contains(const KeyType& Key) const
	{
		return mMap.find(Key) != mMap.end();
	}

	bool IsEmpty() const
	{
		return mMap.empty();
	}

	void Reserve(int32 Capacity)
	{
		mMap.reserve(static_cast<size_t>(Capacity));
	}

	bool RemoveAndCopyValue(const KeyType& Key, ValueType& OutRemovedValue)
	{
		ValueType* Value = Find(Key);
		if (!Value)
			return false;

		OutRemovedValue = std::move(*Value);
		Remove(Key);
		return true;
	}

	ValueType& operator[](const KeyType& Key)
	{
		return mMap[Key];
	}

	const ValueType& operator[](const KeyType& Key) const
	{
		return mMap.at(Key);
	}

private:
	MapType mMap;
};
