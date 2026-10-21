#pragma once

struct FArchiveFieldName
{
	const char* Name;
	
	FArchiveFieldName(const char* InName)
		:Name(InName) {}
};

template <typename T>
struct TNamedValue
{
	FArchiveFieldName Name;
	T& Value;
};

template <typename T>
struct TNamedAttribute
{
	FArchiveFieldName Name;
	T& Value;
};

//template <typename T>
//struct TOptionalNamedAttribute
//{
//	FArchiveFieldName Name;
//	T& Value;
//	const T& Default;
//};

template <typename T>
TNamedValue<T> MakeNamedValue(FArchiveFieldName Name, T& Value)
{
	return TNamedValue<T>{ Name, Value };
}

template <typename T>
TNamedAttribute<T> MakeNamedAttribute(FArchiveFieldName Name, T& Value)
{
	return TNamedAttribute<T>{ Name, Value };
}

//template <typename T>
//TOptionalNamedAttribute<T> MakeOptionalNamedAttribute(FArchiveFieldName Name, T& Value, const typename TIdentity<T>::Type& Default)
//{
//	return TOptionalNamedAttribute<T>{ Name, Value, Default };
//}

#define SA_VALUE(Name, Value) MakeNamedValue(FArchiveFieldName(Name), Value)

#define SA_ATTRIBUTE(Name, Value) MakeNamedAttribute(FArchiveFieldName(Name), Value)

//#define SA_OPTIONAL_ATTRIBUTE(Name, Value, Default) UE::StructuredArchive::Private::MakeOptionalNamedAttribute(FArchiveFieldName(Name), Value, Default)