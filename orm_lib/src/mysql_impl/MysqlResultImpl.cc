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

Result::SizeType MysqlResultImpl::size() const noexcept
{
    return _rowsNum;
}
Result::RowSizeType MysqlResultImpl::columns() const noexcept
{
    return _fieldNum;
}
const char *MysqlResultImpl::columnName(RowSizeType number) const
{
    assert(number < _fieldNum);
    if (_fieldArray)
        return _fieldArray[number].name;
    return "";
}
Result::SizeType MysqlResultImpl::affectedRows() const noexcept
{
    return _affectedRows;
}
Result::RowSizeType MysqlResultImpl::columnNumber(const char colName[]) const
{
    if (!_fieldMapPtr)
        return -1;
    std::string col(colName);
    std::transform(col.begin(), col.end(), col.begin(), tolower);
    if (_fieldMapPtr->find(col) != _fieldMapPtr->end())
        return (*_fieldMapPtr)[col];
    return -1;
}
const char *MysqlResultImpl::getValue(SizeType row, RowSizeType column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return NULL;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    return (*_rowsPtr)[row].first[column];
}
bool MysqlResultImpl::isNull(SizeType row, RowSizeType column) const
{
    return getValue(row, column) == NULL;
}
Result::FieldSizeType MysqlResultImpl::getLength(SizeType row,
                                                   RowSizeType column) const
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
