#pragma once

#include <cassert>
#include <optional>
#include <utility>

// UE의 TOptional 축소판. 값이 있을 수도, 없을 수도 있는 상태를 표현한다.
template <typename T>
class TOptional
{
public:
	TOptional() = default;
	TOptional(const T& InValue) : Value(InValue) {}
	TOptional(T&& InValue) : Value(std::move(InValue)) {}

	bool IsSet() const { return Value.has_value(); }
	explicit operator bool() const { return IsSet(); }

	// 값이 없으면 assert. IsSet()으로 먼저 확인한다.
	T& GetValue() { assert(IsSet()); return *Value; }
	const T& GetValue() const { assert(IsSet()); return *Value; }

	// 값이 없으면 DefaultValue를 반환한다.
	const T& Get(const T& DefaultValue) const { return IsSet() ? *Value : DefaultValue; }

	T* GetPtrOrNull() { return IsSet() ? &*Value : nullptr; }
	const T* GetPtrOrNull() const { return IsSet() ? &*Value : nullptr; }

	template <typename... ArgsType>
	T& Emplace(ArgsType&&... Args) { return Value.emplace(std::forward<ArgsType>(Args)...); }

	void Reset() { Value.reset(); }

	T* operator->() { return &GetValue(); }
	const T* operator->() const { return &GetValue(); }
	T& operator*() { return GetValue(); }
	const T& operator*() const { return GetValue(); }

	bool operator==(const TOptional& Other) const { return Value == Other.Value; }
	bool operator!=(const TOptional& Other) const { return !(*this == Other); }

private:
	std::optional<T> Value;
};
