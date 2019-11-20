/**
 *
 *  Field.cc
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

#include <drogon/orm/Field.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include <stdlib.h>

using namespace drogon::orm;
Field::Field(const Row &row, Row::SizeType columnNum) noexcept
    : row_(Result::SizeType(row.index_)),
      column_(columnNum),
      result_(row.result_)
{
}

const char *Field::name() const
{
    return result_.columnName(column_);
}

bool Field::isNull() const
{
    return result_.isNull(row_, column_);
}

template <>
std::string Field::as<std::string>() const
{
    if (result_.oid(column_) != 17)
    {
        auto data_ = result_.getValue(row_, column_);
        auto dataLength_ = result_.getLength(row_, column_);
        //    LOG_DEBUG << "dataLength_=" << dataLength_ << " str=" << data_;
        return std::string(data_, dataLength_);
    }
    else
    {
        // Bytea type of PostgreSQL
        auto sv = as<string_view>();
        if (sv.length() < 2 || sv[0] != '\\' || sv[1] != 'x')
            return std::string();
        return utils::hexToBinaryString(sv.data() + 2, sv.length() - 2);
    }
}
template <>
const char *Field::as<const char *>() const
{
    auto data_ = result_.getValue(row_, column_);
    return data_;
}
template <>
char *Field::as<char *>() const
{
    auto data_ = result_.getValue(row_, column_);
    return (char *)data_;
}
template <>
std::vector<char> Field::as<std::vector<char>>() const
{
    if (result_.oid(column_) != 17)
    {
        char *first = (char *)result_.getValue(row_, column_);
        char *last = first + result_.getLength(row_, column_);
        return std::vector<char>(first, last);
    }
    else
    {
        // Bytea type of PostgreSQL
        auto sv = as<string_view>();
        if (sv.length() < 2 || sv[0] != '\\' || sv[1] != 'x')
            return std::vector<char>();
        return utils::hexToBinaryVector(sv.data() + 2, sv.length() - 2);
    }
}

const char *Field::c_str() const
{
    return as<const char *>();
}
// template <>
// std::vector<short> Field::as<std::vector<short>>() const
// {
//     auto data_ = result_.getValue(row_, column_);
//     auto dataLength_ = result_.getLength(row_, column_);
//     LOG_DEBUG<<"dataLength_="<<dataLength_;
//     for(int i=0;i<dataLength_;i++)
//     {
//         LOG_DEBUG<<"data["<<i<<"]="<<(int)data_[i];
//     }
//     LOG_DEBUG<<data_;
//     return std::vector<short>((short *)data_,(short *)(data_ + dataLength_));
// }

// template <>
// std::vector<int32_t> Field::as<std::vector<int32_t>>() const
// {
//     auto data_ = result_.getValue(row_, column_);
//     auto dataLength_ = result_.getLength(row_, column_);
//     return std::vector<int32_t>((int32_t *)data_,(int32_t *)(data_ +
//     dataLength_));
// }

// template <>
// std::vector<int64_t> Field::as<std::vector<int64_t>>() const
// {
//     auto data_ = result_.getValue(row_, column_);
//     auto dataLength_ = result_.getLength(row_, column_);
//     return std::vector<int64_t>((int64_t *)data_,(int64_t *)(data_ +
//     dataLength_));
// }
