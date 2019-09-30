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
Field::Field(const Row &row, Row::size_type columnNum) noexcept
    : _row(Result::size_type(row._index)),
      _column(columnNum),
      _result(row._result)
{
}

const char *Field::name() const
{
    return _result.columnName(_column);
}

bool Field::isNull() const
{
    return _result.isNull(_row, _column);
}

template <>
std::string Field::as<std::string>() const
{
    if (_result.oid(_column) != 17)
    {
        auto _data = _result.getValue(_row, _column);
        auto _dataLength = _result.getLength(_row, _column);
        //    LOG_DEBUG << "_dataLength=" << _dataLength << " str=" << _data;
        return std::string(_data, _dataLength);
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
    auto _data = _result.getValue(_row, _column);
    return _data;
}
template <>
char *Field::as<char *>() const
{
    auto _data = _result.getValue(_row, _column);
    return (char *)_data;
}
template <>
std::vector<char> Field::as<std::vector<char>>() const
{
    if (_result.oid(_column) != 17)
    {
        char *first = (char *)_result.getValue(_row, _column);
        char *last = first + _result.getLength(_row, _column);
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
//     auto _data = _result.getValue(_row, _column);
//     auto _dataLength = _result.getLength(_row, _column);
//     LOG_DEBUG<<"_dataLength="<<_dataLength;
//     for(int i=0;i<_dataLength;i++)
//     {
//         LOG_DEBUG<<"data["<<i<<"]="<<(int)_data[i];
//     }
//     LOG_DEBUG<<_data;
//     return std::vector<short>((short *)_data,(short *)(_data + _dataLength));
// }

// template <>
// std::vector<int32_t> Field::as<std::vector<int32_t>>() const
// {
//     auto _data = _result.getValue(_row, _column);
//     auto _dataLength = _result.getLength(_row, _column);
//     return std::vector<int32_t>((int32_t *)_data,(int32_t *)(_data +
//     _dataLength));
// }

// template <>
// std::vector<int64_t> Field::as<std::vector<int64_t>>() const
// {
//     auto _data = _result.getValue(_row, _column);
//     auto _dataLength = _result.getLength(_row, _column);
//     return std::vector<int64_t>((int64_t *)_data,(int64_t *)(_data +
//     _dataLength));
// }
