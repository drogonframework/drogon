/**
 *
 *  OStringStream.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once
#include <string>
#include <sstream>
#include <string_view>

namespace drogon
{
namespace internal
{
template <typename T>
struct CanConvertToString
{
    using Type = std::remove_reference_t<std::remove_cv_t<T>>;

  private:
    using yes = std::true_type;
    using no = std::false_type;

    template <typename U>
    static auto test(int) -> decltype(std::to_string(U{}), yes());

    template <typename>
    static no test(...);

  public:
    static constexpr bool value = std::is_same_v<decltype(test<Type>(0)), yes>;
};
}  // namespace internal

class OStringStream
{
  public:
    OStringStream() = default;

    void reserve(size_t size)
    {
        buffer_.reserve(size);
    }

    template <typename T>
    OStringStream &operator<<(T &&value)
    {
        if constexpr (internal::CanConvertToString<T>::value)
        {
            buffer_.append(std::to_string(std::forward<T>(value)));
            return *this;
        }
        else
        {
            std::stringstream ss;
            ss << std::forward<T>(value);
            buffer_.append(ss.str());
            return *this;
        }
    }

    template <int N>
    OStringStream &operator<<(const char (&buf)[N])
    {
        buffer_.append(buf, N - 1);
        return *this;
    }

    OStringStream &operator<<(const std::string_view &str)
    {
        buffer_.append(str.data(), str.length());
        return *this;
    }

    OStringStream &operator<<(std::string_view &&str)
    {
        buffer_.append(str.data(), str.length());
        return *this;
    }

    OStringStream &operator<<(const std::string &str)
    {
        buffer_.append(str);
        return *this;
    }

    OStringStream &operator<<(std::string &&str)
    {
        buffer_.append(std::move(str));
        return *this;
    }

    OStringStream &operator<<(const double &d)
    {
        std::stringstream ss;
        ss << d;
        buffer_.append(ss.str());
        return *this;
    }

    OStringStream &operator<<(const float &f)
    {
        std::stringstream ss;
        ss << f;
        buffer_.append(ss.str());
        return *this;
    }

    OStringStream &operator<<(double &&d)
    {
        std::stringstream ss;
        ss << d;
        buffer_.append(ss.str());
        return *this;
    }

    OStringStream &operator<<(float &&f)
    {
        std::stringstream ss;
        ss << f;
        buffer_.append(ss.str());
        return *this;
    }

    std::string &str()
    {
        return buffer_;
    }

    const std::string &str() const
    {
        return buffer_;
    }

  private:
    std::string buffer_;
};
}  // namespace drogon
