#pragma once

#include <cstdint>
#include <memory>

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

template <typename T>
using TUniquePtr = std::unique_ptr<T>;

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T>
using TWeakPtr= std::weak_ptr<T>;

template<typename T, typename... Args>
TSharedPtr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
TUniquePtr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

#define DEFINE_ENUM_OPERATORS(EnumName) \
    constexpr EnumName operator&(EnumName _a, EnumName _b) \
    { \
        using UnderlyingType = std::underlying_type_t<EnumName>; \
        return static_cast<EnumName>(static_cast<UnderlyingType>(_a) & static_cast<UnderlyingType>(_b)); \
    } \
    constexpr EnumName operator|(EnumName _a, EnumName _b) \
    { \
        using UnderlyingType = std::underlying_type_t<EnumName>; \
        return static_cast<EnumName>(static_cast<UnderlyingType>(_a) | static_cast<UnderlyingType>(_b)); \
    } \
    constexpr EnumName operator^(EnumName _a, EnumName _b) \
    { \
        using UnderlyingType = std::underlying_type_t<EnumName>; \
        return static_cast<EnumName>(static_cast<UnderlyingType>(_a) ^ static_cast<UnderlyingType>(_b)); \
    } \
    constexpr EnumName operator~(EnumName _a) \
    { \
        using UnderlyingType = std::underlying_type_t<EnumName>; \
        return static_cast<EnumName>(~static_cast<UnderlyingType>(_a)); \
    } \
    inline EnumName& operator&=(EnumName& _a, EnumName _b) \
    { \
        return _a = _a & _b; \
    } \
    inline EnumName& operator|=(EnumName& _a, EnumName _b) \
    { \
        return _a = _a | _b; \
    } \
    inline EnumName& operator^=(EnumName& _a, EnumName _b) \
    { \
        return _a = _a ^ _b; \
    } \
    constexpr bool HasFlag(EnumName _bitMask, EnumName _flag) \
    { \
        using UnderlyingType = std::underlying_type_t<EnumName>; \
        return (static_cast<UnderlyingType>(_bitMask) & static_cast<UnderlyingType>(_flag)) != 0; \
    }