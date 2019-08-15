/**
 *
 *  Sqlite3ResultImpl.h
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

#pragma once

#include "../ResultImpl.h"

#include <sqlite3.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace drogon
{
namespace orm
{
class Sqlite3ResultImpl : public ResultImpl
{
  public:
    explicit Sqlite3ResultImpl(const std::string &query) noexcept
        : ResultImpl(query)
    {
    }
    virtual size_type size() const noexcept override;
    virtual row_size_type columns() const noexcept override;
    virtual const char *columnName(row_size_type number) const override;
    virtual size_type affectedRows() const noexcept override;
    virtual row_size_type columnNumber(const char colName[]) const override;
    virtual const char *getValue(size_type row,
                                 row_size_type column) const override;
    virtual bool isNull(size_type row, row_size_type column) const override;
    virtual field_size_type getLength(size_type row,
                                      row_size_type column) const override;
    virtual unsigned long long insertId() const noexcept override;

  private:
    friend class Sqlite3Connection;
    std::vector<std::vector<std::shared_ptr<std::string>>> _result;
    std::string _query;
    std::vector<std::string> _columnNames;
    std::unordered_map<std::string, size_t> _columnNameMap;
    size_t _affectedRows = 0;
    size_t _insertId = 0;
};
}  // namespace orm
}  // namespace drogon