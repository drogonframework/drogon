/**
 *
 *  Field.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#include <drogon/orm/Field.h>
#include <trantor/utils/Logger.h>

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
    auto _data = _result.getValue(_row, _column);
    auto _dataLength = _result.getLength(_row, _column);
    //    LOG_DEBUG << "_dataLength=" << _dataLength << " str=" << _data;
    return std::string(_data, _dataLength);
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
    char *first = (char *)_result.getValue(_row, _column);
    char *last = first + _result.getLength(_row, _column);
    return std::vector<char>(first, last);
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
//     return std::vector<int32_t>((int32_t *)_data,(int32_t *)(_data + _dataLength));
// }

// template <>
// std::vector<int64_t> Field::as<std::vector<int64_t>>() const
// {
//     auto _data = _result.getValue(_row, _column);
//     auto _dataLength = _result.getLength(_row, _column);
//     return std::vector<int64_t>((int64_t *)_data,(int64_t *)(_data + _dataLength));
// }
