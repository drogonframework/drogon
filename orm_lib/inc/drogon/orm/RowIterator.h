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
class ConstRowIterator : protected Field
{
  public:
    using pointer = const Field *;
    using reference = const Field &;
    using value_type = const Field;
    using size_type = Row::SizeType;
    using difference_type = Row::DifferenceType;
    using iterator_category = std::random_access_iterator_tag;

    // ConstRowIterator(const Field &t) noexcept : Field(t) {}

    pointer operator->()
    {
        return this;
    }

    reference operator*()
    {
        return *this;
    }

    ConstRowIterator operator++(int);

    ConstRowIterator &operator++()
    {
        ++column_;
        return *this;
    }

    ConstRowIterator operator--(int);

    ConstRowIterator &operator--()
    {
        --column_;
        return *this;
    }

    ConstRowIterator &operator+=(difference_type i)
    {
        column_ += i;
        return *this;
    }

    ConstRowIterator &operator-=(difference_type i)
    {
        column_ -= i;
        return *this;
    }

    bool operator==(const ConstRowIterator &other) const
    {
        return column_ == other.column_;
    }

    bool operator!=(const ConstRowIterator &other) const
    {
        return column_ != other.column_;
    }

    bool operator>(const ConstRowIterator &other) const
    {
        return column_ > other.column_;
    }

    bool operator<(const ConstRowIterator &other) const
    {
        return column_ < other.column_;
    }

    bool operator>=(const ConstRowIterator &other) const
    {
        return column_ >= other.column_;
    }

    bool operator<=(const ConstRowIterator &other) const
    {
        return column_ <= other.column_;
    }

  private:
    friend class Row;

    ConstRowIterator(const Row &r, SizeType column) noexcept : Field(r, column)
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
    using iterator_type::reference;

    // using iterator_type::value_type;

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
        return column_ == other.column_;
    }

    bool operator!=(const ConstReverseRowIterator &other) const
    {
        return column_ != other.column_;
    }

    bool operator>(const ConstReverseRowIterator &other) const
    {
        return column_ < other.column_;
    }

    bool operator<(const ConstReverseRowIterator &other) const
    {
        return column_ > other.column_;
    }

    bool operator>=(const ConstReverseRowIterator &other) const
    {
        return column_ <= other.column_;
    }

    bool operator<=(const ConstReverseRowIterator &other) const
    {
        return column_ >= other.column_;
    }
};

}  // namespace orm
}  // namespace drogon
