/**
 *
 *  Field.h
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

// Taken from libpqxx and modified.
// The license for libpqxx can be found in the COPYING file.

#pragma once

#include <drogon/orm/ArrayParser.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <memory>
#include <sstream>
#include <string>
#include <trantor/utils/config.h>
#include <vector>

#ifdef __linux__
#include <arpa/inet.h>
inline uint64_t ntohll(const uint64_t &input)
{
    uint64_t rval;
    uint8_t *data = (uint8_t *)&rval;

    data[0] = input >> 56;
    data[1] = input >> 48;
    data[2] = input >> 40;
    data[3] = input >> 32;
    data[4] = input >> 24;
    data[5] = input >> 16;
    data[6] = input >> 8;
    data[7] = input >> 0;

    return rval;
}

inline uint64_t htonll(const uint64_t &input)
{
    return (ntohll(input));
}
#endif
#ifdef _WIN32
inline uint64_t ntohll(const uint64_t &input)
{
    uint64_t rval;
    uint8_t *data = (uint8_t *)&rval;

    data[0] = input >> 56;
    data[1] = input >> 48;
    data[2] = input >> 40;
    data[3] = input >> 32;
    data[4] = input >> 24;
    data[5] = input >> 16;
    data[6] = input >> 8;
    data[7] = input >> 0;

    return rval;
}

inline uint64_t htonll(const uint64_t &input)
{
    return (ntohll(input));
}
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
class Field
{
  public:
    using size_type = unsigned long;

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

    /// Convert to a type T value
    template <typename T>
    T as() const
    {
        auto _data = _result.getValue(_row, _column);
        // auto _dataLength = _result.getLength(_row, _column);
        // For binary format!
        // if (_dataLength == 1)
        // {
        //     return *_data;
        // }
        // else if (_dataLength == 4)
        // {
        //     const int32_t *n = (int32_t *)_data;
        //     return ntohl(*n);
        // }
        // else if (_dataLength == 8)
        // {
        //     const int64_t *n = (int64_t *)_data;
        //     return ntohll(*n);
        // }
        // return 0;
        T value = T();
        if (_data)
        {
            std::stringstream ss(_data);
            ss >> value;
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
        return ArrayParser(_result.getValue(_row, _column));
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
    Result::size_type _row;
    /**
     * Column number
     * You'd expect this to be a size_t, but due to the way reverse iterators
     * are related to regular iterators, it must be allowed to underflow to -1.
     */
    long _column;
    friend class Row;
    Field(const Row &row, Row::size_type columnNum) noexcept;

  private:
    const Result _result;
};
template <>
std::string Field::as<std::string>() const;
template <>
const char *Field::as<const char *>() const;
template <>
char *Field::as<char *>() const;
template <>
std::vector<char> Field::as<std::vector<char>>() const;
template <>
string_view Field::as<string_view>() const;
// template <>
// std::vector<int32_t> Field::as<std::vector<int32_t>>() const;
// template <>
// std::vector<int64_t> Field::as<std::vector<int64_t>>() const;
}  // namespace orm
}  // namespace drogon
