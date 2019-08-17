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

#include <memory>
#include <string>
#ifdef _WIN32
#define noexcept _NOEXCEPT
#endif
namespace drogon
{
namespace orm
{
class ConstResultIterator;
class ConstReverseResultIterator;
class Row;
class ResultImpl;
typedef std::shared_ptr<ResultImpl> ResultImplPtr;

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
 *	for (result::size_type i=0; i < R.size(); ++i) Process(R[i]);
 * @endcode
 *
 * Result sets in libpqxx are lightweight, reference-counted wrapper objects
 * which are relatively small and cheap to copy.  Think of a result object as
 * a "smart pointer" to an underlying result set.
 */
class Result
{
  public:
    Result(const ResultImplPtr &ptr) : _resultPtr(ptr)
    {
    }
    using difference_type = long;
    using size_type = unsigned long;
    using reference = Row;
    using ConstIterator = ConstResultIterator;
    using Iterator = ConstIterator;
    using row_size_type = unsigned long;
    using field_size_type = unsigned long;

    using ConstReverseIterator = ConstReverseResultIterator;
    using ReverseIterator = ConstReverseIterator;

    size_type size() const noexcept;
    size_type capacity() const noexcept
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

    reference front() const noexcept;
    reference back() const noexcept;

    reference operator[](size_type index) const;
    reference at(size_type index) const;
    void swap(Result &) noexcept;

    /// Number of columns in result.
    row_size_type columns() const noexcept;

    /// Name of column with this number (throws exception if it doesn't exist)
    const char *columnName(row_size_type number) const;

    /// If command was @c INSERT, @c UPDATE, or @c DELETE: number of affected
    /// rows
    /**
     * @return Number of affected rows if last command was @c INSERT, @c UPDATE,
     * or @c DELETE; zero for all other commands.
     */
    size_type affectedRows() const noexcept;

    /// For Mysql, Sqlite3 databases, return the auto-incrementing primary key
    /// after inserting
    /**
     * For postgreSQL databases, this method always returns zero, One can use
     * the following sql to get auto-incrementing id:
     *   insert into table_name volumn1, volumn2 values(....) returning id;
     */
    unsigned long long insertId() const noexcept;

    /// Query that produced this result, if available (empty string otherwise)
    const std::string &sql() const noexcept;

  private:
    ResultImplPtr _resultPtr;

    friend class Field;
    friend class Row;
    /// Number of given column (throws exception if it doesn't exist).
    row_size_type columnNumber(const char colName[]) const;
    /// Number of given column (throws exception if it doesn't exist).
    row_size_type columnNumber(const std::string &name) const
    {
        return columnNumber(name.c_str());
    }
    /// Get the column oid, for postgresql database
    int oid(row_size_type column) const noexcept;

    const char *getValue(size_type row, row_size_type column) const;
    bool isNull(size_type row, row_size_type column) const;
    field_size_type getLength(size_type row, row_size_type column) const;

  protected:
    Result()
    {
    }
};
inline void swap(Result &one, Result &two) noexcept
{
    one.swap(two);
}
}  // namespace orm
}  // namespace drogon

namespace std
{
template <>
inline void swap<drogon::orm::Result>(drogon::orm::Result &one,
                                      drogon::orm::Result &two) noexcept
{
    one.swap(two);
}
}  // namespace std
