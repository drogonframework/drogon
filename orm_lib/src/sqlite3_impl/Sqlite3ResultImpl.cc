/**
 *
 *  Sqlite3ResultImpl.cc
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

#include "Sqlite3ResultImpl.h"
#include <assert.h>
#include <algorithm>

using namespace drogon::orm;

Result::size_type Sqlite3ResultImpl::size() const noexcept
{
    return _result.size();
}
Result::row_size_type Sqlite3ResultImpl::columns() const noexcept
{
    return _result.empty() ? 0 : _result[0].size();
}
const char *Sqlite3ResultImpl::columnName(row_size_type number) const
{
    assert(number < _columnNames.size());
    return _columnNames[number].c_str();
}
Result::size_type Sqlite3ResultImpl::affectedRows() const noexcept
{
    return _affectedRows;
}
Result::row_size_type Sqlite3ResultImpl::columnNumber(const char colName[]) const
{
    auto name = std::string(colName);
    std::transform(name.begin(), name.end(), name.begin(), tolower);
    auto iter = _columnNameMap.find(name);
    if (iter != _columnNameMap.end())
    {
        return iter->second;
    }
    throw std::string("there is no column named ") + colName;
}
const char *Sqlite3ResultImpl::getValue(size_type row, row_size_type column) const
{
    auto col = _result[row][column];
    return col ? col->c_str() : nullptr;
}
bool Sqlite3ResultImpl::isNull(size_type row, row_size_type column) const
{
    return !_result[row][column];
}
Result::field_size_type Sqlite3ResultImpl::getLength(size_type row, row_size_type column) const
{
    auto col = _result[row][column];
    return col ? col->length() : 0;
}
unsigned long long Sqlite3ResultImpl::insertId() const noexcept
{
    return _insertId;
}
