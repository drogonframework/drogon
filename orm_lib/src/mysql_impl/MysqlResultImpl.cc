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
#include <cassert>
#include <drogon/orm/Exception.h>

using namespace drogon::orm;

Result::SizeType MysqlResultImpl::size() const noexcept
{
    return rowsNumber_;
}

Result::RowSizeType MysqlResultImpl::columns() const noexcept
{
    return fieldsNumber_;
}

const char *MysqlResultImpl::columnName(RowSizeType number) const
{
    assert(number < fieldsNumber_);
    if (fieldArray_)
        return fieldArray_[number].name;
    return "";
}

Result::SizeType MysqlResultImpl::affectedRows() const noexcept
{
    return affectedRows_;
}

Result::RowSizeType MysqlResultImpl::columnNumber(const char colName[]) const
{
    if (!fieldsMapPtr_)
        return -1;
    std::string col(colName);
    std::transform(col.begin(), col.end(), col.begin(), [](unsigned char c) {
        return tolower(c);
    });
    if (fieldsMapPtr_->find(col) != fieldsMapPtr_->end())
        return (*fieldsMapPtr_)[col];
    throw RangeError(std::string("no column named ") + colName);
}

const char *MysqlResultImpl::getValue(SizeType row, RowSizeType column) const
{
    if (rowsNumber_ == 0 || fieldsNumber_ == 0)
        return NULL;
    assert(row < rowsNumber_);
    assert(column < fieldsNumber_);
    return (*rowsPtr_)[row].first[column];
}

bool MysqlResultImpl::isNull(SizeType row, RowSizeType column) const
{
    return getValue(row, column) == NULL;
}

Result::FieldSizeType MysqlResultImpl::getLength(SizeType row,
                                                 RowSizeType column) const
{
    if (rowsNumber_ == 0 || fieldsNumber_ == 0)
        return 0;
    assert(row < rowsNumber_);
    assert(column < fieldsNumber_);
    return (*rowsPtr_)[row].second[column];
}

unsigned long long MysqlResultImpl::insertId() const noexcept
{
    return insertId_;
}
