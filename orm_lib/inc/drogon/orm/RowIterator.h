/**
 *
 *  RowIterator.h
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

#include <drogon/orm/Field.h>
#include <drogon/orm/Row.h>
namespace drogon
{
namespace orm
{
class ConstRowIterator : public std::iterator<std::random_access_iterator_tag,
                                              const Field,
                                              Row::difference_type,
                                              ConstRowIterator,
                                              Field>,
                         protected Field
{
  public:
    using pointer = const Field *;
    using reference = Field;
    using size_type = Row::size_type;
    using difference_type = Row::difference_type;
    // ConstRowIterator(const Field &t) noexcept : Field(t) {}

    pointer operator->()
    {
        return this;
    }
    reference operator*()
    {
        return Field(*this);
    }

    ConstRowIterator operator++(int);
    ConstRowIterator &operator++()
    {
        ++_column;
        return *this;
    }
    ConstRowIterator operator--(int);
    ConstRowIterator &operator--()
    {
        --_column;
        return *this;
    }
    ConstRowIterator &operator+=(difference_type i)
    {
        _column += i;
        return *this;
    }
    ConstRowIterator &operator-=(difference_type i)
    {
        _column -= i;
        return *this;
    }

    bool operator==(const ConstRowIterator &other) const
    {
        return _column == other._column;
    }
    bool operator!=(const ConstRowIterator &other) const
    {
        return _column != other._column;
    }
    bool operator>(const ConstRowIterator &other) const
    {
        return _column > other._column;
    }
    bool operator<(const ConstRowIterator &other) const
    {
        return _column < other._column;
    }
    bool operator>=(const ConstRowIterator &other) const
    {
        return _column >= other._column;
    }
    bool operator<=(const ConstRowIterator &other) const
    {
        return _column <= other._column;
    }

  private:
    friend class Row;
    ConstRowIterator(const Row &r, size_type column) noexcept : Field(r, column)
    {
    }
};

class ConstReverseRowIterator : private ConstRowIterator
{
  public:
    using super = ConstRowIterator;
    using iterator_type = ConstRowIterator;
    using iterator_type::difference_type;
    using iterator_type::iterator_category;
    using iterator_type::pointer;
    using value_type = iterator_type::value_type;
    using reference = iterator_type::reference;

    ConstReverseRowIterator(const ConstReverseRowIterator &rhs)
        : ConstRowIterator(rhs)
    {
    }
    explicit ConstReverseRowIterator(const ConstRowIterator &rhs)
        : ConstRowIterator(rhs)
    {
        super::operator--();
    }

    ConstRowIterator base() const noexcept;

    using iterator_type::operator->;
    using iterator_type::operator*;

    ConstReverseRowIterator operator++(int);
    ConstReverseRowIterator &operator++()
    {
        iterator_type::operator--();
        return *this;
    }
    ConstReverseRowIterator operator--(int);
    ConstReverseRowIterator &operator--()
    {
        iterator_type::operator++();
        return *this;
    }
    ConstReverseRowIterator &operator+=(difference_type i)
    {
        iterator_type::operator-=(i);
        return *this;
    }
    ConstReverseRowIterator &operator-=(difference_type i)
    {
        iterator_type::operator+=(i);
        return *this;
    }

    bool operator==(const ConstReverseRowIterator &other) const
    {
        return _column == other._column;
    }
    bool operator!=(const ConstReverseRowIterator &other) const
    {
        return _column != other._column;
    }
    bool operator>(const ConstReverseRowIterator &other) const
    {
        return _column < other._column;
    }
    bool operator<(const ConstReverseRowIterator &other) const
    {
        return _column > other._column;
    }
    bool operator>=(const ConstReverseRowIterator &other) const
    {
        return _column <= other._column;
    }
    bool operator<=(const ConstReverseRowIterator &other) const
    {
        return _column >= other._column;
    }
};

}  // namespace orm
}  // namespace drogon
