/**
 *
 *  ResultImpl.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */
#pragma once

#include <trantor/utils/NonCopyable.h>
#include <drogon/orm/Result.h>
namespace drogon
{
namespace orm
{

class ResultImpl : public trantor::NonCopyable, public Result
{
public:
  virtual size_type size() const noexcept = 0;
  virtual row_size_type columns() const noexcept = 0;
  virtual const char *columnName(row_size_type Number) const = 0;
  virtual size_type affectedRows() const noexcept = 0;
  virtual row_size_type columnNumber(const char colName[]) const = 0;
  virtual const char *getValue(size_type row, row_size_type column) const = 0;
  virtual bool isNull(size_type row, row_size_type column) const = 0;
  virtual field_size_type getLength(size_type row, row_size_type column) const = 0;
  virtual ~ResultImpl() {}
};

} // namespace orm
} // namespace drogon
