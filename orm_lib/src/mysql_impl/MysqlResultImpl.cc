/**
 *
 *  MysqlResultImpl.cc
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

#include "MysqlResultImpl.h"
#include <algorithm>
#include <assert.h>

using namespace drogon::orm;

Result::size_type MysqlResultImpl::size() const noexcept
{
    return _rowsNum;
}
Result::row_size_type MysqlResultImpl::columns() const noexcept
{
    return _fieldNum;
}
const char *MysqlResultImpl::columnName(row_size_type number) const
{
    assert(number < _fieldNum);
    if (_fieldArray)
        return _fieldArray[number].name;
    return "";
}
Result::size_type MysqlResultImpl::affectedRows() const noexcept
{
    return _affectedRows;
}
Result::row_size_type MysqlResultImpl::columnNumber(const char colName[]) const
{
    if (!_fieldMapPtr)
        return -1;
    std::string col(colName);
    std::transform(col.begin(), col.end(), col.begin(), tolower);
    if (_fieldMapPtr->find(col) != _fieldMapPtr->end())
        return (*_fieldMapPtr)[col];
    return -1;
}
const char *MysqlResultImpl::getValue(size_type row, row_size_type column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return NULL;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    return (*_rowsPtr)[row].first[column];
}
bool MysqlResultImpl::isNull(size_type row, row_size_type column) const
{
    return getValue(row, column) == NULL;
}
Result::field_size_type MysqlResultImpl::getLength(size_type row,
                                                   row_size_type column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return 0;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    return (*_rowsPtr)[row].second[column];
}
unsigned long long MysqlResultImpl::insertId() const noexcept
{
    return _insertId;
}
