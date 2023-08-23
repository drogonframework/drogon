/**
 *
 *  Result.cc
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

#include "ResultImpl.h"
#include <cassert>
#include <drogon/orm/Result.h>
#include <drogon/orm/ResultIterator.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Exception.h>

using namespace drogon::orm;

Result::ConstIterator Result::begin() const noexcept
{
    return ConstIterator(*this, (SizeType)0);
}

Result::ConstIterator Result::cbegin() const noexcept
{
    return begin();
}

Result::ConstIterator Result::end() const noexcept
{
    return ConstIterator(*this, size());
}

Result::ConstIterator Result::cend() const noexcept
{
    return end();
}

Result::ConstReverseIterator Result::rbegin() const
{
    return ConstReverseResultIterator(end());
}

Result::ConstReverseIterator Result::crbegin() const
{
    return rbegin();
}

Result::ConstReverseIterator Result::rend() const
{
    return ConstReverseResultIterator(begin());
}

Result::ConstReverseIterator Result::crend() const
{
    return rend();
}

Result::ConstIterator Result::ConstReverseIterator::base() const noexcept
{
    iterator_type tmp(*this);
    return ++tmp;
}

Result::Reference Result::front() const noexcept
{
    return Row(*this, 0);
}

Result::Reference Result::back() const noexcept
{
    return Row(*this, size() - 1);
}

Result::Reference Result::operator[](SizeType index) const noexcept
{
    assert(index < size());
    return Row(*this, index);
}

Result::Reference Result::at(SizeType index) const
{
    if (index >= size())
        throw RangeError("Result index is out of range");
    return operator[](index);
}

ConstResultIterator ConstResultIterator::operator++(int)
{
    ConstResultIterator old(*this);
    ++index_;
    return old;
}

ConstResultIterator ConstResultIterator::operator--(int)
{
    ConstResultIterator old(*this);
    --index_;
    return old;
}

ConstReverseResultIterator ConstReverseResultIterator::operator++(int)
{
    ConstReverseResultIterator old(*this);
    iterator_type::operator--();
    return old;
}

ConstReverseResultIterator ConstReverseResultIterator::operator--(int)
{
    ConstReverseResultIterator old(*this);
    iterator_type::operator++();
    return old;
}

Result::SizeType Result::size() const noexcept
{
    return resultPtr_->size();
}

void Result::swap(Result &other) noexcept
{
    resultPtr_.swap(other.resultPtr_);
}

Result::RowSizeType Result::columns() const noexcept
{
    return resultPtr_->columns();
}

const char *Result::columnName(Result::RowSizeType number) const
{
    return resultPtr_->columnName(number);
}

Result::SizeType Result::affectedRows() const noexcept
{
    return resultPtr_->affectedRows();
}

Result::RowSizeType Result::columnNumber(const char colName[]) const
{
    return resultPtr_->columnNumber(colName);
}

const char *Result::getValue(Result::SizeType row,
                             Result::RowSizeType column) const
{
    return resultPtr_->getValue(row, column);
}

bool Result::isNull(Result::SizeType row, Result::RowSizeType column) const
{
    return resultPtr_->isNull(row, column);
}

Result::FieldSizeType Result::getLength(Result::SizeType row,
                                        Result::RowSizeType column) const
{
    return resultPtr_->getLength(row, column);
}

unsigned long long Result::insertId() const noexcept
{
    return resultPtr_->insertId();
}

int Result::oid(RowSizeType column) const noexcept
{
    return resultPtr_->oid(column);
}

Result &Result::operator=(const Result &r) noexcept
{
    resultPtr_ = r.resultPtr_;
    return *this;
}

Result &Result::operator=(Result &&r) noexcept
{
    resultPtr_ = std::move(r.resultPtr_);
    return *this;
}
