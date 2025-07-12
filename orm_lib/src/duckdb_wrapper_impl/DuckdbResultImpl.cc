/**
 *
 *  DuckdbResultImpl.cc
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

#include "DuckdbResultImpl.h"
#include <algorithm>
#include <cassert>
#include <drogon/orm/Exception.h>

using namespace drogon::orm;

Result::SizeType DuckdbResultImpl::size() const noexcept
{
    return result_.size();
}

Result::RowSizeType DuckdbResultImpl::columns() const noexcept
{
    return result_.empty() ? 0 : result_[0].size();
}

const char *DuckdbResultImpl::columnName(RowSizeType number) const
{
    assert(number < columnNames_.size());
    return columnNames_[number].c_str();
}

Result::SizeType DuckdbResultImpl::affectedRows() const noexcept
{
    return affectedRows_;
}

Result::RowSizeType DuckdbResultImpl::columnNumber(const char colName[]) const
{
    auto name = std::string(colName);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return tolower(c);
    });
    auto iter = columnNamesMap_.find(name);
    if (iter != columnNamesMap_.end())
    {
        return iter->second;
    }
    throw RangeError(std::string("there is no column named ") + colName);
}

const char *DuckdbResultImpl::getValue(SizeType row, RowSizeType column) const
{
    auto col = result_[row][column];
    return col ? col->c_str() : nullptr;
}

bool DuckdbResultImpl::isNull(SizeType row, RowSizeType column) const
{
    return !result_[row][column];
}

Result::FieldSizeType DuckdbResultImpl::getLength(SizeType row,
                                                   RowSizeType column) const
{
    auto col = result_[row][column];
    return col ? col->length() : 0;
}

unsigned long long DuckdbResultImpl::insertId() const noexcept
{
    return insertId_;
}
