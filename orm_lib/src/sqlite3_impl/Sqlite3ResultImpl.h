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
#include <vector>

namespace drogon
{
namespace orm
{
class Sqlite3ResultImpl : public ResultImpl
{
  public:
    Sqlite3ResultImpl(const std::string &query) noexcept
        : _query(query),
          _result(new std::vector<std::vector<std::shared_ptr<std::string>>>)
    {

    }
    virtual size_type size() const noexcept override;
    virtual row_size_type columns() const noexcept override;
    virtual const char *columnName(row_size_type Number) const override;
    virtual size_type affectedRows() const noexcept override;
    virtual row_size_type columnNumber(const char colName[]) const override;
    virtual const char *getValue(size_type row, row_size_type column) const override;
    virtual bool isNull(size_type row, row_size_type column) const override;
    virtual field_size_type getLength(size_type row, row_size_type column) const override;

  private:
    friend class Sqlite3Connection;
    std::shared_ptr<std::vector<std::vector<std::shared_ptr<std::string>>>> _result;
    std::string _query;
    size_t _affectedRows = 0;
};
} // namespace orm
} // namespace drogon