/**
 *
 *  MysqlStmtResultImpl.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#include "MysqlStmtResultImpl.h"
#include <assert.h>
#include <algorithm>

using namespace drogon::orm;

Result::size_type MysqlStmtResultImpl::size() const noexcept
{
    return _rowsNum;
}
Result::row_size_type MysqlStmtResultImpl::columns() const noexcept
{
    return _fieldNum;
}
const char *MysqlStmtResultImpl::columnName(row_size_type number) const
{
    assert(number < _fieldNum);
    if (_fieldArray)
        return _fieldArray[number].name;
    return "";
}
Result::size_type MysqlStmtResultImpl::affectedRows() const noexcept
{
    return _affectedRows;
}
Result::row_size_type MysqlStmtResultImpl::columnNumber(const char colName[]) const
{
    std::string col(colName);
    std::transform(col.begin(), col.end(), col.begin(), tolower);
    auto iter = _fieldMap.find(col);
    if (iter != _fieldMap.end())
        return iter->second;
    return -1;
}
const char *MysqlStmtResultImpl::getValue(size_type row, row_size_type column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return NULL;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    return _rows[row].first[column];
}
bool MysqlStmtResultImpl::isNull(size_type row, row_size_type column) const
{
    return getValue(row, column) == NULL;
}
Result::field_size_type MysqlStmtResultImpl::getLength(size_type row, row_size_type column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return 0;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    return _rows[row].second[column];
}
