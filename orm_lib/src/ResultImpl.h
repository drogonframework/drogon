/**
 *
 *  ResultImpl.h
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

#include <drogon/orm/Result.h>
#include <trantor/utils/NonCopyable.h>
namespace drogon
{
namespace orm
{
class ResultImpl : public trantor::NonCopyable
{
  public:
    ResultImpl() = default;
    using SizeType = Result::SizeType;
    using RowSizeType = Result::RowSizeType;
    using FieldSizeType = Result::FieldSizeType;
    virtual SizeType size() const noexcept = 0;
    virtual RowSizeType columns() const noexcept = 0;
    virtual const char *columnName(RowSizeType Number) const = 0;
    virtual SizeType affectedRows() const noexcept = 0;
    virtual RowSizeType columnNumber(const char colName[]) const = 0;
    virtual const char *getValue(SizeType row, RowSizeType column) const = 0;
    virtual bool isNull(SizeType row, RowSizeType column) const = 0;
    virtual FieldSizeType getLength(SizeType row, RowSizeType column) const = 0;

    virtual unsigned long long insertId() const noexcept
    {
        return 0;
    }
    virtual int oid(RowSizeType column) const
    {
        (void)column;
        return 0;
    }
    virtual ~ResultImpl()
    {
    }
};

}  // namespace orm
}  // namespace drogon
