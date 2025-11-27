/**
 *
 *  @file DuckdbResultImpl.h
 *  @author Dq Wei
 *
 *  Copyright 2025, Dq Wei.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include "../ResultImpl.h"

#include <duckdb.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace drogon
{
namespace orm
{
class DuckdbResultImpl : public ResultImpl
{
  public:
    DuckdbResultImpl(std::shared_ptr<duckdb_result> r,
                     SizeType affectedRows,
                     unsigned long long insertId) noexcept;

    SizeType size() const noexcept override;
    RowSizeType columns() const noexcept override;
    const char *columnName(RowSizeType number) const override;
    SizeType affectedRows() const noexcept override;
    RowSizeType columnNumber(const char colName[]) const override;
    const char *getValue(SizeType row, RowSizeType column) const override;
    bool isNull(SizeType row, RowSizeType column) const override;
    FieldSizeType getLength(SizeType row, RowSizeType column) const override;
    unsigned long long insertId() const noexcept override;

  private:
    friend class DuckdbConnection;

    // Use shared_ptr to manage duckdb_result, reference MySQL implementation
    const std::shared_ptr<duckdb_result> result_;

    // Cached metadata
    const Result::SizeType rowsNumber_;
    const Result::RowSizeType columnsNumber_;
    const SizeType affectedRows_;
    const unsigned long long insertId_;

    // Column name mapping (lowercase, supports case-insensitive queries)
    std::shared_ptr<std::unordered_map<std::string, RowSizeType>>
        columnNamesMap_;

    // Value cache (convert on demand, avoid repeated conversions)
    mutable std::unordered_map<std::string, std::shared_ptr<std::string>>
        valueCache_;

    // Helper function: convert values on demand
    std::shared_ptr<std::string> convertValue(RowSizeType col,
                                               SizeType row) const;
};
}  // namespace orm
}  // namespace drogon
