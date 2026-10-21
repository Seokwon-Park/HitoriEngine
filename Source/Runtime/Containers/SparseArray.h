#pragma once

#include <vector>
#include <utility>

template<typename T>
class TSparseArray
{
public:
	class Iterator;
	class ConstIterator;

	TSparseArray() = default;
	~TSparseArray() = default;

	T& operator[](uint32 index);
	const T& operator[](uint32 index) const;

	Iterator begin();
	Iterator end();

	ConstIterator begin() const;
	ConstIterator end() const;

	uint32 Add(const T& data);
	uint32 Emplace(const T& data);

	int32 Num() const;
	void Reserve(uint32 capacity);
	int32 Size() const;
	int32 Max() const;

	bool IsEmpty() const;
	bool IsValidIndex(uint32 index) const;

	void Reset(int32 newSize);
	void RemoveAt(uint32 index);

	TArray<T> ToTArray() const;

	class Iterator
	{
		TSparseArray* Owner;
		std::size_t Index;

		void SkipEmpty();

	public:
		Iterator(TSparseArray* owner, std::size_t index);

		T& operator*() const;
		T* operator->() const;

		Iterator& operator++();
		bool operator==(const Iterator& other) const;
		bool operator!=(const Iterator& other) const;
	};

	class ConstIterator
	{
		TSparseArray* Owner;
		std::size_t Index;

		void SkipEmpty();

	public:
		ConstIterator(TSparseArray* owner, std::size_t index);

		const T& operator*() const;
		const T* operator->() const;

		ConstIterator& operator++();
		bool operator==(const ConstIterator& other) const;
		bool operator!=(const ConstIterator& other) const;
	};

	union Slot
	{
		T Element;
		int32 NextFreeIndex;
	};

private:
	std::vector<std::pair<bool, Slot>> mDatas;
	int32 mFreeIndex = -1;
	int32 mNumElements = 0;
};


