/**
 *
 *  ResultIterator.h
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

// Taken from libpqxx and modified.
// The license for libpqxx can be found in the COPYING file.

#pragma once

#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
namespace drogon
{
namespace orm
{
class ConstResultIterator
    : public std::iterator<std::random_access_iterator_tag,
                           const Row,
                           Result::difference_type,
                           ConstResultIterator,
                           Row>,
      protected Row
{
  public:
    using pointer = const Row *;
    using reference = Row;
    using size_type = Result::size_type;
    using difference_type = Result::difference_type;
    // ConstResultIterator(const Row &t) noexcept : Row(t) {}

    pointer operator->()
    {
        return this;
    }
    reference operator*()
    {
        return Row(*this);
    }

    ConstResultIterator operator++(int);
    ConstResultIterator &operator++()
    {
        ++_index;
        return *this;
    }
    ConstResultIterator operator--(int);
    ConstResultIterator &operator--()
    {
        --_index;
        return *this;
    }
    ConstResultIterator &operator+=(difference_type i)
    {
        _index += i;
        return *this;
    }
    ConstResultIterator &operator-=(difference_type i)
    {
        _index -= i;
        return *this;
    }

    bool operator==(const ConstResultIterator &other) const
    {
        return _index == other._index;
    }
    bool operator!=(const ConstResultIterator &other) const
    {
        return _index != other._index;
    }
    bool operator>(const ConstResultIterator &other) const
    {
        return _index > other._index;
    }
    bool operator<(const ConstResultIterator &other) const
    {
        return _index < other._index;
    }
    bool operator>=(const ConstResultIterator &other) const
    {
        return _index >= other._index;
    }
    bool operator<=(const ConstResultIterator &other) const
    {
        return _index <= other._index;
    }

  private:
    friend class Result;
    ConstResultIterator(const Result &r, size_type index) noexcept
        : Row(r, index)
    {
    }
};

class ConstReverseResultIterator : private ConstResultIterator
{
  public:
    using super = ConstResultIterator;
    using iterator_type = ConstResultIterator;
    using iterator_type::difference_type;
    using iterator_type::iterator_category;
    using iterator_type::pointer;
    using value_type = iterator_type::value_type;
    using reference = iterator_type::reference;

    ConstReverseResultIterator(const ConstReverseResultIterator &rhs)
        : ConstResultIterator(rhs)
    {
    }
    explicit ConstReverseResultIterator(const ConstResultIterator &rhs)
        : ConstResultIterator(rhs)
    {
        super::operator--();
    }

    ConstResultIterator base() const noexcept;

    using iterator_type::operator->;
    using iterator_type::operator*;

    ConstReverseResultIterator operator++(int);
    ConstReverseResultIterator &operator++()
    {
        iterator_type::operator--();
        return *this;
    }
    ConstReverseResultIterator operator--(int);
    ConstReverseResultIterator &operator--()
    {
        iterator_type::operator++();
        return *this;
    }
    ConstReverseResultIterator &operator+=(difference_type i)
    {
        iterator_type::operator-=(i);
        return *this;
    }
    ConstReverseResultIterator &operator-=(difference_type i)
    {
        iterator_type::operator+=(i);
        return *this;
    }

    bool operator==(const ConstReverseResultIterator &other) const
    {
        return _index == other._index;
    }
    bool operator!=(const ConstReverseResultIterator &other) const
    {
        return _index != other._index;
    }
    bool operator>(const ConstReverseResultIterator &other) const
    {
        return _index < other._index;
    }
    bool operator<(const ConstReverseResultIterator &other) const
    {
        return _index > other._index;
    }
    bool operator>=(const ConstReverseResultIterator &other) const
    {
        return _index <= other._index;
    }
    bool operator<=(const ConstReverseResultIterator &other) const
    {
        return _index >= other._index;
    }
};

}  // namespace orm
}  // namespace drogon
