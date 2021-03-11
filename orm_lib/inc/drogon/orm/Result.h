/**
 *
 *  Result.h
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
class Result
{
  public:
    Result(const ResultImplPtr &ptr) : resultPtr_(ptr)
    {
    }
    DROGON_EXPORT Result(const Result &r) noexcept = default;
    DROGON_EXPORT Result(Result &&) noexcept = default;
    DROGON_EXPORT Result &operator=(const Result &r) noexcept;
    DROGON_EXPORT Result &operator=(Result &&) noexcept;
    using DifferenceType = long;
    using SizeType = unsigned long;
    using Reference = Row;
    using ConstIterator = ConstResultIterator;
    using Iterator = ConstIterator;
    using RowSizeType = unsigned long;
    using FieldSizeType = unsigned long;

    using ConstReverseIterator = ConstReverseResultIterator;
    using ReverseIterator = ConstReverseIterator;

    DROGON_EXPORT SizeType size() const noexcept;
    SizeType capacity() const noexcept
    {
        return size();
    }
    DROGON_EXPORT ConstIterator begin() const noexcept;
    DROGON_EXPORT ConstIterator cbegin() const noexcept;
    DROGON_EXPORT ConstIterator end() const noexcept;
    DROGON_EXPORT ConstIterator cend() const noexcept;

    DROGON_EXPORT ConstReverseIterator rbegin() const;
    DROGON_EXPORT ConstReverseIterator crbegin() const;
    DROGON_EXPORT ConstReverseIterator rend() const;
    DROGON_EXPORT ConstReverseIterator crend() const;

    bool empty() const noexcept
    {
        return size() == 0;
    }

    DROGON_EXPORT Reference front() const noexcept;
    DROGON_EXPORT Reference back() const noexcept;

    DROGON_EXPORT Reference operator[](SizeType index) const noexcept;
    DROGON_EXPORT Reference at(SizeType index) const;
    DROGON_EXPORT void swap(Result &) noexcept;

    /// Number of columns in result.
    DROGON_EXPORT RowSizeType columns() const noexcept;

    /// Name of column with this number (throws exception if it doesn't exist)
    DROGON_EXPORT const char *columnName(RowSizeType number) const;

    /// If command was @c INSERT, @c UPDATE, or @c DELETE: number of affected
    /// rows
    /**
     * @return Number of affected rows if last command was @c INSERT, @c UPDATE,
     * or @c DELETE; zero for all other commands.
     */
    DROGON_EXPORT SizeType affectedRows() const noexcept;

    /// For Mysql, Sqlite3 databases, return the auto-incrementing primary key
    /// after inserting
    /**
     * For postgreSQL databases, this method always returns zero, One can use
     * the following sql to get auto-incrementing id:
     *   insert into table_name volumn1, volumn2 values(....) returning id;
     */
    DROGON_EXPORT unsigned long long insertId() const noexcept;

#ifdef _MSC_VER
    Result() = default;
#endif

  private:
    ResultImplPtr resultPtr_;

    friend class Field;
    friend class Row;
    /// Number of given column (throws exception if it doesn't exist).
    DROGON_EXPORT RowSizeType columnNumber(const char colName[]) const;
    /// Number of given column (throws exception if it doesn't exist).
    RowSizeType columnNumber(const std::string &name) const
    {
        return columnNumber(name.c_str());
    }
    /// Get the column oid, for postgresql database
    DROGON_EXPORT int oid(RowSizeType column) const noexcept;

    DROGON_EXPORT const char *getValue(SizeType row, RowSizeType column) const;
    DROGON_EXPORT bool isNull(SizeType row, RowSizeType column) const;
    DROGON_EXPORT FieldSizeType getLength(SizeType row, RowSizeType column) const;
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
