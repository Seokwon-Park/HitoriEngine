#pragma once

#include <cassert>
#include <unordered_set>
#include "Core/Types.h"


template <typename T>
class TSet
{
public:
	TSet() = default;
	~TSet() = default;

	std::unordered_set<T>::iterator begin();
	std::unordered_set<T>::iterator end();

	std::unordered_set<T>::const_iterator begin() const;
	std::unordered_set<T>::const_iterator end() const;

	bool Add(const T& data);

	// Returns removed count
	int32 Remove(const T& data);

	int32  Num() const;
	void Reset();

	bool Contains(const T& data) const;
	bool IsEmpty() const;
	void Reserve(int32 capacity);

	/*
	void Empty(int32 ExpectedNumElements = 0)
	*/

private:
	std::unordered_set<T> mSet;
};

template<typename T>
inline typename std::unordered_set<T>::iterator TSet<T>::begin() { return mSet.begin(); }

template<typename T>
inline typename std::unordered_set<T>::iterator TSet<T>::end() { return mSet.end(); }

template<typename T>
inline typename std::unordered_set<T>::const_iterator TSet<T>::begin() const { return mSet.cbegin(); }

template<typename T>
inline typename std::unordered_set<T>::const_iterator TSet<T>::end() const { return mSet.cend(); }

template<typename T>
inline bool TSet<T>::Add(const T& data) { return mSet.insert(data).second; }

template<typename T>
inline int32 TSet<T>::Remove(const T& data) { return static_cast<int32>(mSet.erase(data)); }

template<typename T> 
inline int32 TSet<T>::Num() const { return static_cast<int32>(mSet.size()); }

template<typename T> 
inline void TSet<T>::Reset() { mSet.clear(); }

template<typename T> 
inline bool TSet<T>::Contains(const T& data) const { return mSet.find(data) != mSet.end(); }

template<typename T> 
inline bool TSet<T>::IsEmpty() const { return mSet.empty(); }

template<typename T> 
inline void TSet<T>::Reserve(int32 capacity) { mSet.reserve(capacity); }
