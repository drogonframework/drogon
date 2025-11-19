/**
 *
 *  DuckdbResultImpl.h
 *  Dq wei
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

    // 使用智能指针管理 duckdb_result，参考 MySQL 实现
    const std::shared_ptr<duckdb_result> result_;

    // 缓存的元数据
    const Result::SizeType rowsNumber_;
    const Result::RowSizeType columnsNumber_;
    const SizeType affectedRows_;
    const unsigned long long insertId_;

    // 列名映射（小写，支持大小写不敏感查询）
    std::shared_ptr<std::unordered_map<std::string, RowSizeType>>
        columnNamesMap_;

    // 值缓存（按需转换，避免重复转换）
    mutable std::unordered_map<std::string, std::shared_ptr<std::string>>
        valueCache_;

    // 辅助函数：按需转换值
    std::shared_ptr<std::string> convertValue(RowSizeType col,
                                               SizeType row) const;
};
}  // namespace orm
}  // namespace drogon
