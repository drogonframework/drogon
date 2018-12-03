/**
 *
 *  Result.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

//Taken from libpqxx and modified.
//The license for libpqxx can be found in the COPYING file.

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
class Result
{
  public:
    Result(const ResultImplPtr &ptr)
        : _resultPtr(ptr) {}
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
    size_type capacity() const noexcept { return size(); }
    ConstIterator begin() const noexcept;
    ConstIterator cbegin() const noexcept;
    ConstIterator end() const noexcept;
    ConstIterator cend() const noexcept;

    ConstReverseIterator rbegin() const;
    ConstReverseIterator crbegin() const;
    ConstReverseIterator rend() const;
    ConstReverseIterator crend() const;

    bool empty() const noexcept { return size() == 0; }

    reference front() const noexcept;
    reference back() const noexcept;

    reference operator[](size_type index) const;
    reference at(size_type index) const;
    void swap(Result &) noexcept;
    row_size_type columns() const noexcept;
    /// Name of column with this number (throws exception if it doesn't exist)
    const char *columnName(row_size_type number) const;
    const std::string &errorDescription() const { return _errString; }
    void setError(const std::string &description) { _errString = description; }

    size_type affectedRows() const noexcept;

    /// For Mysql database only
    unsigned long long insertId() const noexcept;

  private:
    ResultImplPtr _resultPtr;
    std::string _query;
    std::string _errString;
    friend class Field;
    friend class Row;
    /// Number of given column (throws exception if it doesn't exist).
    row_size_type columnNumber(const char colName[]) const;
    /// Number of given column (throws exception if it doesn't exist).
    row_size_type columnNumber(const std::string &name) const
    {
        return columnNumber(name.c_str());
    }

    const char *getValue(size_type row, row_size_type column) const;
    bool isNull(size_type row, row_size_type column) const;
    field_size_type getLength(size_type row, row_size_type column) const;

  protected:
    Result() {}
};
} // namespace orm
} // namespace drogon
