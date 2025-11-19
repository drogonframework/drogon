/**
 *
 *  DuckdbResultImpl.cc
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

#include "DuckdbResultImpl.h"
#include <algorithm>
#include <cstring>

using namespace drogon::orm;

DuckdbResultImpl::DuckdbResultImpl(std::shared_ptr<duckdb_result> r,
                                   SizeType affectedRows,
                                   unsigned long long insertId) noexcept
    : result_(std::move(r)),
      rowsNumber_(result_ ? duckdb_row_count(result_.get()) : 0),
      columnsNumber_(result_ ? duckdb_column_count(result_.get()) : 0),
      affectedRows_(affectedRows),
      insertId_(insertId)
{
    if (columnsNumber_ > 0)
    {
        columnNamesMap_ = std::make_shared<
            std::unordered_map<std::string, RowSizeType>>();

        for (RowSizeType i = 0; i < columnsNumber_; ++i)
        {
            const char *colName = duckdb_column_name(result_.get(), i);
            std::string fieldName(colName);
            // 转换为小写以支持大小写不敏感查询
            std::transform(fieldName.begin(),
                          fieldName.end(),
                          fieldName.begin(),
                          [](unsigned char c) { return tolower(c); });
            (*columnNamesMap_)[fieldName] = i;
        }
    }
}

Result::SizeType DuckdbResultImpl::size() const noexcept
{
    return rowsNumber_;
}

Result::RowSizeType DuckdbResultImpl::columns() const noexcept
{
    return columnsNumber_;
}

const char *DuckdbResultImpl::columnName(RowSizeType number) const
{
    if (!result_ || number >= columnsNumber_)
    {
        return nullptr;
    }
    return duckdb_column_name(result_.get(), number);
}

Result::SizeType DuckdbResultImpl::affectedRows() const noexcept
{
    return affectedRows_;
}

Result::RowSizeType DuckdbResultImpl::columnNumber(const char colName[]) const
{
    if (!columnNamesMap_)
    {
        return -1;
    }

    std::string name(colName);
    std::transform(name.begin(),
                   name.end(),
                   name.begin(),
                   [](unsigned char c) { return tolower(c); });

    auto iter = columnNamesMap_->find(name);
    if (iter == columnNamesMap_->end())
    {
        return -1;
    }
    return iter->second;
}

const char *DuckdbResultImpl::getValue(SizeType row, RowSizeType column) const
{
    if (!result_ || row >= rowsNumber_ || column >= columnsNumber_)
    {
        return nullptr;
    }

    // 检查 NULL
    if (duckdb_value_is_null(result_.get(), column, row))
    {
        return nullptr;
    }

    // 生成缓存键
    std::string cacheKey = std::to_string(row) + "_" + std::to_string(column);

    // 检查缓存
    auto it = valueCache_.find(cacheKey);
    if (it != valueCache_.end())
    {
        return it->second->c_str();
    }

    // 按需转换并缓存
    auto value = convertValue(column, row);
    if (value)
    {
        valueCache_[cacheKey] = value;
        return value->c_str();
    }

    return nullptr;
}

bool DuckdbResultImpl::isNull(SizeType row, RowSizeType column) const
{
    if (!result_ || row >= rowsNumber_ || column >= columnsNumber_)
    {
        return true;
    }
    return duckdb_value_is_null(result_.get(), column, row);
}

Result::FieldSizeType DuckdbResultImpl::getLength(SizeType row,
                                                   RowSizeType column) const
{
    const char *val = getValue(row, column);
    return val ? std::strlen(val) : 0;
}

unsigned long long DuckdbResultImpl::insertId() const noexcept
{
    return insertId_;
}

std::shared_ptr<std::string> DuckdbResultImpl::convertValue(
    RowSizeType col,
    SizeType row) const
{
    duckdb_type type = duckdb_column_type(result_.get(), col);

    switch (type)
    {
        case DUCKDB_TYPE_BOOLEAN:
        {
            bool val = duckdb_value_boolean(result_.get(), col, row);
            return std::make_shared<std::string>(val ? "1" : "0");
        }
        case DUCKDB_TYPE_TINYINT:
        {
            int8_t val = duckdb_value_int8(result_.get(), col, row);
            return std::make_shared<std::string>(std::to_string(val));
        }
        case DUCKDB_TYPE_SMALLINT:
        {
            int16_t val = duckdb_value_int16(result_.get(), col, row);
            return std::make_shared<std::string>(std::to_string(val));
        }
        case DUCKDB_TYPE_INTEGER:
        {
            int32_t val = duckdb_value_int32(result_.get(), col, row);
            return std::make_shared<std::string>(std::to_string(val));
        }
        case DUCKDB_TYPE_BIGINT:
        {
            int64_t val = duckdb_value_int64(result_.get(), col, row);
            return std::make_shared<std::string>(std::to_string(val));
        }
        case DUCKDB_TYPE_FLOAT:
        {
            float val = duckdb_value_float(result_.get(), col, row);
            return std::make_shared<std::string>(std::to_string(val));
        }
        case DUCKDB_TYPE_DOUBLE:
        {
            double val = duckdb_value_double(result_.get(), col, row);
            return std::make_shared<std::string>(std::to_string(val));
        }
        case DUCKDB_TYPE_VARCHAR:
        {
            char *val = duckdb_value_varchar(result_.get(), col, row);
            auto str = std::make_shared<std::string>(val);
            duckdb_free(val);  // 立即释放 DuckDB 分配的内存
            return str;
        }
        case DUCKDB_TYPE_BLOB:
        {
            duckdb_blob blob = duckdb_value_blob(result_.get(), col, row);
            auto str = std::make_shared<std::string>(
                static_cast<const char *>(blob.data), blob.size);
            duckdb_free(blob.data);
            return str;
        }
        default:
        {
            // 未知类型尝试转为字符串
            char *val = duckdb_value_varchar(result_.get(), col, row);
            auto str = std::make_shared<std::string>(val);
            duckdb_free(val);
            return str;
        }
    }
}
