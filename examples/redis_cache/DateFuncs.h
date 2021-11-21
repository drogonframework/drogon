#pragma once
#include "RedisCache.h"

template <>
inline trantor::Date fromString<trantor::Date>(const std::string &str)
{
    return trantor::Date(std::atoll(str.data()));
}
template <>
inline std::string toString<trantor::Date>(const trantor::Date &date)
{
    return std::to_string(date.microSecondsSinceEpoch());
}