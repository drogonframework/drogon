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
#include <algorithm>
#include <cassert>
#include <drogon/orm/Exception.h>

using namespace drogon::orm;

Result::SizeType Sqlite3ResultImpl::size() const noexcept
{
    return result_.size();
}

Result::RowSizeType Sqlite3ResultImpl::columns() const noexcept
{
    return result_.empty() ? 0 : result_[0].size();
}

const char *Sqlite3ResultImpl::columnName(RowSizeType number) const
{
    assert(number < columnNames_.size());
    return columnNames_[number].c_str();
}

Result::SizeType Sqlite3ResultImpl::affectedRows() const noexcept
{
    return affectedRows_;
}

Result::RowSizeType Sqlite3ResultImpl::columnNumber(const char colName[]) const
{
    auto name = std::string(colName);
    std::transform(name.begin(), name.end(), name.begin(), tolower);
    auto iter = columnNamesMap_.find(name);
    if (iter != columnNamesMap_.end())
    {
        return iter->second;
    }
    throw RangeError(std::string("there is no column named ") + colName);
}

const char *Sqlite3ResultImpl::getValue(SizeType row, RowSizeType column) const
{
    auto col = result_[row][column];
    return col ? col->c_str() : nullptr;
}

bool Sqlite3ResultImpl::isNull(SizeType row, RowSizeType column) const
{
    return !result_[row][column];
}

Result::FieldSizeType Sqlite3ResultImpl::getLength(SizeType row,
                                                   RowSizeType column) const
{
    auto col = result_[row][column];
    return col ? col->length() : 0;
}

unsigned long long Sqlite3ResultImpl::insertId() const noexcept
{
    return insertId_;
}
