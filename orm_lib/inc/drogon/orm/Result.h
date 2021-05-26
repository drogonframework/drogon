/**
 *
 *  @file Result.h
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

// Taken from libpqxx and modified.
// The license for libpqxx can be found in the COPYING file.

#pragma once

#include <drogon/exports.h>
#include <memory>
#include <string>
#include <future>
#include <algorithm>
#include <assert.h>

namespace drogon
{
namespace orm
{
class ConstResultIterator;
class ConstReverseResultIterator;
class Row;
class ResultImpl;
using ResultImplPtr = std::shared_ptr<ResultImpl>;

enum class SqlStatus
{
    Ok,
    End
};

/// Result set containing data returned by a query or command.
/** This behaves as a container (as defined by the C++ standard library) and
 * provides random access const iterators to iterate over its rows.  A row
 * can also be accessed by indexing a result R by the row's zero-based
 * number:
 *
 * @code
 *	for (Result::SizeType i=0; i < R.size(); ++i) Process(R[i]);
 * @endcode
 *
 * Result sets in libpqxx are lightweight, Reference-counted wrapper objects
 * which are relatively small and cheap to copy.  Think of a result object as
 * a "smart pointer" to an underlying result set.
 */
class DROGON_EXPORT Result
{
  public:
    Result(const ResultImplPtr &ptr) : resultPtr_(ptr)
    {
    }
    Result(const Result &r) noexcept = default;
    Result(Result &&) noexcept = default;
    Result &operator=(const Result &r) noexcept;
    Result &operator=(Result &&) noexcept;
    using DifferenceType = long;
    using SizeType = size_t;
    using Reference = Row;
    using ConstIterator = ConstResultIterator;
    using Iterator = ConstIterator;
    using RowSizeType = unsigned long;
    using FieldSizeType = unsigned long;

    using ConstReverseIterator = ConstReverseResultIterator;
    using ReverseIterator = ConstReverseIterator;

    SizeType size() const noexcept;
    SizeType capacity() const noexcept
    {
        return size();
    }
    ConstIterator begin() const noexcept;
    ConstIterator cbegin() const noexcept;
    ConstIterator end() const noexcept;
    ConstIterator cend() const noexcept;

    ConstReverseIterator rbegin() const;
    ConstReverseIterator crbegin() const;
    ConstReverseIterator rend() const;
    ConstReverseIterator crend() const;

    bool empty() const noexcept
    {
        return size() == 0;
    }

    Reference front() const noexcept;
    Reference back() const noexcept;

    Reference operator[](SizeType index) const noexcept;
    Reference at(SizeType index) const;
    void swap(Result &) noexcept;

    /// Number of columns in result.
    RowSizeType columns() const noexcept;

    /// Name of column with this number (throws exception if it doesn't exist)
    const char *columnName(RowSizeType number) const;

    /// If command was @c INSERT, @c UPDATE, or @c DELETE: number of affected
    /// rows
    /**
     * @return Number of affected rows if last command was @c INSERT, @c UPDATE,
     * or @c DELETE; zero for all other commands.
     */
    SizeType affectedRows() const noexcept;

    /// For Mysql, Sqlite3 databases, return the auto-incrementing primary key
    /// after inserting
    /**
     * For postgreSQL databases, this method always returns zero, One can use
     * the following sql to get auto-incrementing id:
     *   insert into table_name volumn1, volumn2 values(....) returning id;
     */
    unsigned long long insertId() const noexcept;

#ifdef _MSC_VER
    Result() = default;
#endif

  private:
    ResultImplPtr resultPtr_;

    friend class Field;
    friend class Row;
    /// Number of given column (throws exception if it doesn't exist).
    RowSizeType columnNumber(const char colName[]) const;
    /// Number of given column (throws exception if it doesn't exist).
    RowSizeType columnNumber(const std::string &name) const
    {
        return columnNumber(name.c_str());
    }
    /// Get the column oid, for postgresql database
    int oid(RowSizeType column) const noexcept;

    const char *getValue(SizeType row, RowSizeType column) const;
    bool isNull(SizeType row, RowSizeType column) const;
    FieldSizeType getLength(SizeType row, RowSizeType column) const;
};
inline void swap(Result &one, Result &two) noexcept
{
    one.swap(two);
}
}  // namespace orm
}  // namespace drogon

#ifndef _MSC_VER
namespace std
{
template <>
inline void swap(drogon::orm::Result &one, drogon::orm::Result &two) noexcept
{
    one.swap(two);
}
}  // namespace std
#endif
