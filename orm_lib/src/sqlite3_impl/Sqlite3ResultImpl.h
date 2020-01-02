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
    Sqlite3ResultImpl() = default;
    virtual SizeType size() const noexcept override;
    virtual RowSizeType columns() const noexcept override;
    virtual const char *columnName(RowSizeType number) const override;
    virtual SizeType affectedRows() const noexcept override;
    virtual RowSizeType columnNumber(const char colName[]) const override;
    virtual const char *getValue(SizeType row,
                                 RowSizeType column) const override;
    virtual bool isNull(SizeType row, RowSizeType column) const override;
    virtual FieldSizeType getLength(SizeType row,
                                    RowSizeType column) const override;
    virtual unsigned long long insertId() const noexcept override;

  private:
    friend class Sqlite3Connection;
    std::vector<std::vector<std::shared_ptr<std::string>>> result_;
    std::string query_;
    std::vector<std::string> columnNames_;
    std::unordered_map<std::string, size_t> columnNamesMap_;
    size_t affectedRows_{0};
    size_t insertId_{0};
};
}  // namespace orm
}  // namespace drogon