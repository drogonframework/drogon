/**
 *
 *  Field.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

//Taken from libpqxx and modified.
//The license for libpqxx can be found in the COPYING file.

#pragma once

#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/ArrayParser.h>
#include <vector>
#include <string>
#include <sstream>
#include <memory>

#ifdef __linux__
#include <arpa/inet.h>
inline uint64_t
ntohll(const uint64_t &input)
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

inline uint64_t
htonll(const uint64_t &input)
{
    return (ntohll(input));
}
#endif
#ifdef _WIN32
inline uint64_t
ntohll(const uint64_t &input)
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

inline uint64_t
htonll(const uint64_t &input)
{
    return (ntohll(input));
}
#endif
namespace drogon
{
namespace orm
{
class Field
{
  public:
    using size_type = unsigned long;
    const char *name() const;
    bool isNull() const;
    template <typename T>
    T as() const
    {
        auto _data = _result.getValue(_row, _column);
        //auto _dataLength = _result.getLength(_row, _column);
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
    ArrayParser getArrayParser() const
    {
        return ArrayParser(_result.getValue(_row, _column));
    }
    template <typename T>
    std::vector<std::shared_ptr<T>> asArray() const
    {
        std::vector<std::shared_ptr<T>> ret;
        auto arrParser = getArrayParser();
        while(1)
        {
            auto arrVal = arrParser.getNext();
            if(arrVal.first==ArrayParser::juncture::done)
            {
                break;
            }
            if(arrVal.first==ArrayParser::juncture::string_value)
            {
                T val;
                std::stringstream ss(std::move(arrVal.second));
                ss >> val;
                ret.push_back(std::shared_ptr<T>(new T(val)));
            }
            else if(arrVal.first==ArrayParser::juncture::null_value)
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
    Result _result;
};
template <>
std::string Field::as<std::string>() const;
template <>
const char *Field::as<const char *>() const;
template <>
char *Field::as<char *>() const;
// template <>
// std::vector<short> Field::as<std::vector<short>>() const;
// template <>
// std::vector<int32_t> Field::as<std::vector<int32_t>>() const;
// template <>
// std::vector<int64_t> Field::as<std::vector<int64_t>>() const;
} // namespace orm
} // namespace drogon
