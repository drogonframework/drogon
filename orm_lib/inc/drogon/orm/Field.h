/**
 *
 *  @file Field.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

// Taken from libpqxx and modified.
// The license for libpqxx can be found in the COPYING file.

#pragma once

#include <drogon/exports.h>
#include <drogon/utils/string_view.h>
#include <drogon/orm/ArrayParser.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <trantor/utils/Logger.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#ifdef __linux__
#include <arpa/inet.h>
#endif

namespace drogon
{
namespace orm
{
/// Reference to a field in a result set.
/**
 * A field represents one entry in a row.  It represents an actual value
 * in the result set, and can be converted to various types.
 */
class DROGON_EXPORT Field
{
  public:
    using SizeType = unsigned long;

    /// Column name
    const char *name() const;

    /// Is this field's value null?
    bool isNull() const;

    /// Read as plain C string
    /**
     * Since the field's data is stored internally in the form of a
     * zero-terminated C string, this is the fastest way to read it.  Use the
     * to() or as() functions to convert the string to other types such as
     * @c int, or to C++ strings.
     */
    const char *c_str() const;

    /// Get the length of the plain C string
    size_t length() const
    {
        return result_.getLength(row_, column_);
    }

    /// Convert to a type T value
    template <typename T>
    T as() const
    {
        if (isNull())
            return T();
        auto data_ = result_.getValue(row_, column_);
        // auto dataLength_ = result_.getLength(row_, column_);
        // For binary format!
        // if (dataLength_ == 1)
        // {
        //     return *data_;
        // }
        // else if (dataLength_ == 4)
        // {
        //     const int32_t *n = (int32_t *)data_;
        //     return ntohl(*n);
        // }
        // else if (dataLength_ == 8)
        // {
        //     const int64_t *n = (int64_t *)data_;
        //     return ntohll(*n);
        // }
        // return 0;
        T value = T();
        if (data_)
        {
            try
            {
                std::stringstream ss(data_);
                ss >> value;
            }
            catch (...)
            {
                LOG_DEBUG << "Type error";
            }
        }
        return value;
    }

    /// Parse the field as an SQL array.
    /**
     * Call the parser to retrieve values (and structure) from the array.
     *
     * Make sure the @c result object stays alive until parsing is finished.  If
     * you keep the @c row of @c field object alive, it will keep the @c result
     * object alive as well.
     */
    ArrayParser getArrayParser() const
    {
        return ArrayParser(result_.getValue(row_, column_));
    }
    template <typename T>
    std::vector<std::shared_ptr<T>> asArray() const
    {
        std::vector<std::shared_ptr<T>> ret;
        auto arrParser = getArrayParser();
        while (1)
        {
            auto arrVal = arrParser.getNext();
            if (arrVal.first == ArrayParser::juncture::done)
            {
                break;
            }
            if (arrVal.first == ArrayParser::juncture::string_value)
            {
                T val;
                std::stringstream ss(std::move(arrVal.second));
                ss >> val;
                ret.push_back(std::shared_ptr<T>(new T(val)));
            }
            else if (arrVal.first == ArrayParser::juncture::null_value)
            {
                ret.push_back(std::shared_ptr<T>());
            }
        }
        return ret;
    }

  protected:
    Result::SizeType row_;
    /**
     * Column number
     * You'd expect this to be a size_t, but due to the way reverse iterators
     * are related to regular iterators, it must be allowed to underflow to -1.
     */
    long column_;
    friend class Row;
    Field(const Row &row, Row::SizeType columnNum) noexcept;

  private:
    const Result result_;
};
template <>
DROGON_EXPORT std::string Field::as<std::string>() const;
template <>
DROGON_EXPORT const char *Field::as<const char *>() const;
template <>
DROGON_EXPORT char *Field::as<char *>() const;
template <>
DROGON_EXPORT std::vector<char> Field::as<std::vector<char>>() const;
template <>
inline drogon::string_view Field::as<drogon::string_view>() const
{
    auto first = result_.getValue(row_, column_);
    auto length = result_.getLength(row_, column_);
    return drogon::string_view(first, length);
}

template <>
inline float Field::as<float>() const
{
    if (isNull())
        return 0.0;
    return std::stof(result_.getValue(row_, column_));
}

template <>
inline double Field::as<double>() const
{
    if (isNull())
        return 0.0;
    return std::stod(result_.getValue(row_, column_));
}

template <>
inline bool Field::as<bool>() const
{
    if (result_.getLength(row_, column_) != 1)
    {
        return false;
    }
    auto value = result_.getValue(row_, column_);
    if (*value == 't' || *value == '1')
        return true;
    return false;
}

template <>
inline int Field::as<int>() const
{
    if (isNull())
        return 0;
    return std::stoi(result_.getValue(row_, column_));
}

template <>
inline long Field::as<long>() const
{
    if (isNull())
        return 0;
    return std::stol(result_.getValue(row_, column_));
}

template <>
inline int8_t Field::as<int8_t>() const
{
    if (isNull())
        return 0;
    return static_cast<int8_t>(atoi(result_.getValue(row_, column_)));
}

template <>
inline long long Field::as<long long>() const
{
    if (isNull())
        return 0;
    return atoll(result_.getValue(row_, column_));
}

template <>
inline unsigned int Field::as<unsigned int>() const
{
    if (isNull())
        return 0;
    return static_cast<unsigned int>(
        std::stoi(result_.getValue(row_, column_)));
}

template <>
inline unsigned long Field::as<unsigned long>() const
{
    if (isNull())
        return 0;
    return std::stoul(result_.getValue(row_, column_));
}

template <>
inline uint8_t Field::as<uint8_t>() const
{
    if (isNull())
        return 0;
    return static_cast<uint8_t>(atoi(result_.getValue(row_, column_)));
}

template <>
inline unsigned long long Field::as<unsigned long long>() const
{
    if (isNull())
        return 0;
    return std::stoull(result_.getValue(row_, column_));
}

// std::vector<int32_t> Field::as<std::vector<int32_t>>() const;
// template <>
// std::vector<int64_t> Field::as<std::vector<int64_t>>() const;
}  // namespace orm
}  // namespace drogon
