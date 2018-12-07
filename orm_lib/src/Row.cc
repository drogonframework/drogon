/**
 *
 *  Row.cc
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

#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/RowIterator.h>
using namespace drogon::orm;
Row::Row(const Result &r, size_type index) noexcept
    : _result(r),
      _index(long(index)),
      _end(r.columns())
{
}
Row::size_type Row::size() const
{
    return _end;
}
Row::reference Row::operator[](size_type index) const
{
    if (index >= _end)
        throw; //TODO throw....
    return Field(*this, index);
}

Row::reference Row::operator[](const char columnName[]) const
{
    auto N = _result.columnNumber(columnName);
    return Field(*this, N);
}

Row::reference Row::operator[](const std::string &columnName) const
{
    return operator[](columnName.c_str());
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
    return ConstIterator(*this, size());
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
    _column++;
    return old;
}
ConstRowIterator ConstRowIterator::operator--(int)
{
    ConstRowIterator old(*this);
    _column--;
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
