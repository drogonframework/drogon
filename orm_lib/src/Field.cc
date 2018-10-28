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
