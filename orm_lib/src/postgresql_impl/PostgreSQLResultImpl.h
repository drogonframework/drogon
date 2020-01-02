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
    PostgreSQLResultImpl(const std::shared_ptr<PGresult> &r) noexcept
        : result_(r)
    {
    }
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
    virtual int oid(RowSizeType column) const override;

  private:
    std::shared_ptr<PGresult> result_;
};

}  // namespace orm
}  // namespace drogon
