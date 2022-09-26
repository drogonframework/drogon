/**
 *
 *  PostgreSQLResultImpl.h
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

#include <libpq-fe.h>
#include <memory>
#include <string>

namespace drogon
{
namespace orm
{
class PostgreSQLResultImpl : public ResultImpl
{
  public:
    explicit PostgreSQLResultImpl(std::shared_ptr<PGresult> r) noexcept
        : result_(std::move(r))
    {
    }
    SizeType size() const noexcept override;
    RowSizeType columns() const noexcept override;
    const char *columnName(RowSizeType number) const override;
    SizeType affectedRows() const noexcept override;
    RowSizeType columnNumber(const char colName[]) const override;
    const char *getValue(SizeType row, RowSizeType column) const override;
    bool isNull(SizeType row, RowSizeType column) const override;
    FieldSizeType getLength(SizeType row, RowSizeType column) const override;
    int oid(RowSizeType column) const override;

  private:
    std::shared_ptr<PGresult> result_;
};

}  // namespace orm
}  // namespace drogon
