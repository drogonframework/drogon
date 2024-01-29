/**
 *
 *  @file Row.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <cassert>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/RowIterator.h>

using namespace drogon::orm;

Row::Row(const Result &r, SizeType index) noexcept
    : result_(r), index_(long(index)), end_(r.columns())
{
}

Row::SizeType Row::size() const
{
    return end_;
}

Row::Reference Row::operator[](SizeType index) const noexcept
{
    assert(index < end_);
    return Field(*this, index);
}

Row::Reference Row::operator[](int index) const noexcept
{
    assert(index >= 0);
    auto i = static_cast<SizeType>(index);
    assert(i < end_);
    return Field(*this, i);
}

Row::Reference Row::operator[](const char columnName[]) const
{
    auto N = result_.columnNumber(columnName);
    return Field(*this, N);
}

Row::Reference Row::operator[](const std::string &columnName) const
{
    return operator[](columnName.c_str());
}

Row::Reference Row::at(SizeType index) const
{
    if (index >= end_)
        throw RangeError("Row index is out of range");
    return Field(*this, index);
}

Row::Reference Row::at(const char columnName[]) const
{
    auto N = result_.columnNumber(columnName);
    return Field(*this, N);
}

Row::Reference Row::at(const std::string &columnName) const
{
    return at(columnName.c_str());
}

Row::ConstIterator Row::begin() const noexcept
{
    return ConstIterator(*this, 0);
}

Row::ConstIterator Row::cbegin() const noexcept
{
    return begin();
}

Row::ConstIterator Row::end() const noexcept
{
    return ConstIterator(*this, (Field::SizeType)size());
}

Row::ConstIterator Row::cend() const noexcept
{
    return end();
}

Row::ConstReverseIterator Row::rbegin() const
{
    return ConstReverseRowIterator(end());
}

Row::ConstReverseIterator Row::crbegin() const
{
    return rbegin();
}

Row::ConstReverseIterator Row::rend() const
{
    return ConstReverseRowIterator(begin());
}

Row::ConstReverseIterator Row::crend() const
{
    return rend();
}

Row::ConstIterator Row::ConstReverseIterator::base() const noexcept
{
    iterator_type tmp(*this);
    return ++tmp;
}

ConstRowIterator ConstRowIterator::operator++(int)
{
    ConstRowIterator old(*this);
    ++column_;
    return old;
}

ConstRowIterator ConstRowIterator::operator--(int)
{
    ConstRowIterator old(*this);
    --column_;
    return old;
}

ConstReverseRowIterator ConstReverseRowIterator::operator++(int)
{
    ConstReverseRowIterator old(*this);
    iterator_type::operator--();
    return old;
}

ConstReverseRowIterator ConstReverseRowIterator::operator--(int)
{
    ConstReverseRowIterator old(*this);
    iterator_type::operator++();
    return old;
}
