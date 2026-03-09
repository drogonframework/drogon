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

// 🔥 NEW: SQL field type abstraction
enum class SqlFieldType : uint8_t
{
    Unknown = 0,
    Bool,
    Int,
    BigInt,
    Float,
    Double,
    Decimal,
    Varchar,
    Text,
    Date,
    Time,
    DateTime,
    Blob,
    Json,
    Binary
};

enum class SqlStatus
{
    Ok,
    End
};

struct ColumnMeta
{
    SqlFieldType sqlType{SqlFieldType::Unknown};
    std::string typeName;
    int length{0};
    int precision{0};
    int scale{0};
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
    explicit Result(ResultImplPtr ptr) : resultPtr_(std::move(ptr))
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

    // C++ type type definition compatibility
    using value_type = Row;
    using size_type = SizeType;
    using difference_type = DifferenceType;
    using reference = Reference;
    using const_reference = const Reference;
    using iterator = Iterator;
    using const_iterator = ConstIterator;
    using reverse_iterator = ConstReverseIterator;
    using const_reverse_iterator = ConstReverseIterator;

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

    /**
     * @brief Get the logical SQL type of the specified column.
     *
     * @param column Zero-based index of the column (must be less than columns()).
     * @return The abstracted SQL field type for the given column, or
     *         SqlFieldType::Unknown if the type cannot be determined.
     *
     * @note Type metadata may only be fully populated for some database
     *       backends (for example, MySQL). For other backends, the result
     *       may be limited or fall back to SqlFieldType::Unknown.
     */
    SqlFieldType getSqlType(SizeType column) const;

    /**
     * @brief Get the database-specific type name of the specified column.
     *
     * @param column Zero-based index of the column (must be less than columns()).
     * @return A reference to a string containing the type name as reported
     *         by the underlying database driver (for example, "INT",
     *         "VARCHAR", "DECIMAL(10,2)", etc.).
     *
     * @note Type-name metadata may only be fully populated for some database
     *       backends (for example, MySQL). On other backends, the returned
     *       string may be empty or use a backend-specific representation.
     */
    const std::string &getTypeName(SizeType column) const;

    /**
     * @brief Get the defined maximum length of the specified column.
     *
     * @param column Zero-based index of the column (must be less than columns()).
     * @return The maximum length for the column in characters or bytes, as
     *         reported by the underlying database driver, or 0 if this
     *         information is not available.
     *
     * @note Length metadata may only be populated for some database backends
     *       (for example, MySQL) and for certain column types (such as
     *       character and binary types).
     */
    int getColumnLength(SizeType column) const;

    /**
     * @brief Get the numeric precision for the specified column.
     *
     * @param column Zero-based index of the column (must be less than columns()).
     * @return The precision (total number of significant digits) for the
     *         column, as reported by the underlying database driver, or 0 if
     *         not applicable or not available.
     *
     * @note Precision is generally only meaningful for numeric types such as
     *       DECIMAL or NUMERIC. On other types, the value may be 0 or
     *       unspecified, and metadata may only be provided by some backends
     *       (for example, MySQL).
     */
    int getPrecision(SizeType column) const;

    /**
     * @brief Get the numeric scale for the specified column.
     *
     * @param column Zero-based index of the column (must be less than columns()).
     * @return The scale (number of fractional digits) for the column, as
     *         reported by the underlying database driver, or 0 if not
     *         applicable or not available.
     *
     * @note Scale is generally only meaningful for numeric types such as
     *       DECIMAL or NUMERIC. On other types, the value may be 0 or
     *       unspecified, and metadata may only be provided by some backends
     *       (for example, MySQL).
     */
    int getScale(SizeType column) const;

#ifdef _MSC_VER
    Result() noexcept = default;
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
