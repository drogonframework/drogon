/**
 *
 *  MysqlResultImpl.h
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
#include <trantor/utils/Logger.h>
#include <algorithm>
#include <memory>
#include <mysql.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace drogon
{
namespace orm
{

inline SqlFieldType mysqlTypeToSql(enum enum_field_types t, unsigned int flags)
{
    switch (t)
    {
        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_LONG:
            return SqlFieldType::Int;

        case MYSQL_TYPE_LONGLONG:
            return SqlFieldType::BigInt;

        case MYSQL_TYPE_FLOAT:
            return SqlFieldType::Float;

        case MYSQL_TYPE_DOUBLE:
            return SqlFieldType::Double;

        case MYSQL_TYPE_NEWDECIMAL:
            return SqlFieldType::Decimal;

        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
            if (flags & BINARY_FLAG)
                return SqlFieldType::Binary;
            return SqlFieldType::Varchar;

        case MYSQL_TYPE_STRING:
            if (flags & BINARY_FLAG)
                return SqlFieldType::Binary;  // 🔥 THIS FIXES BINARY(16)
            return SqlFieldType::Text;

        case MYSQL_TYPE_BLOB:
            return SqlFieldType::Blob;

        case MYSQL_TYPE_DATE:
            return SqlFieldType::Date;

        case MYSQL_TYPE_TIME:
            return SqlFieldType::Time;

        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
            return SqlFieldType::DateTime;

        case MYSQL_TYPE_JSON:
            return SqlFieldType::Json;

        default:
            return SqlFieldType::Unknown;
    }
}

inline const char *mysqlFieldTypeToName(enum enum_field_types t,
                                        unsigned int flags)
{
    switch (t)
    {
        case MYSQL_TYPE_TINY:
            return "TINYINT";
        case MYSQL_TYPE_SHORT:
            return "SMALLINT";
        case MYSQL_TYPE_LONG:
            return "INT";
        case MYSQL_TYPE_LONGLONG:
            return "BIGINT";
        case MYSQL_TYPE_FLOAT:
            return "FLOAT";
        case MYSQL_TYPE_DOUBLE:
            return "DOUBLE";
        case MYSQL_TYPE_NEWDECIMAL:
            return "DECIMAL";

        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
            return (flags & BINARY_FLAG) ? "VARBINARY" : "VARCHAR";

        case MYSQL_TYPE_STRING:
            return (flags & BINARY_FLAG) ? "BINARY" : "CHAR";

        case MYSQL_TYPE_BLOB:
            return (flags & BINARY_FLAG) ? "BLOB" : "TEXT";

        case MYSQL_TYPE_DATE:
            return "DATE";
        case MYSQL_TYPE_TIME:
            return "TIME";
        case MYSQL_TYPE_DATETIME:
            return "DATETIME";
        case MYSQL_TYPE_TIMESTAMP:
            return "TIMESTAMP";
        case MYSQL_TYPE_JSON:
            return "JSON";

        default:
            return "UNKNOWN";
    }
}

class MysqlResultImpl : public ResultImpl
{
  public:
    MysqlResultImpl(std::shared_ptr<MYSQL_RES> r,
                    SizeType affectedRows,
                    unsigned long long insertId) noexcept
        : result_(std::move(r)),
          rowsNumber_(result_ ? mysql_num_rows(result_.get()) : 0),
          fieldArray_(result_ ? mysql_fetch_fields(result_.get()) : nullptr),
          fieldsNumber_(result_ ? mysql_num_fields(result_.get()) : 0),
          affectedRows_(affectedRows),
          insertId_(insertId)
    {
        if (fieldArray_ && fieldsNumber_ > 0)
        {
            columnMeta_.resize(fieldsNumber_);

            for (unsigned int i = 0; i < fieldsNumber_; ++i)
            {
                const MYSQL_FIELD &f = fieldArray_[i];
                auto &meta = columnMeta_[i];

                meta.sqlType = mysqlTypeToSql(f.type, f.flags);

                // mysql_field_type_to_name() may return nullptr
                const char *typeName = mysqlFieldTypeToName(f.type, f.flags);
                meta.typeName = typeName ? typeName : "UNKNOWN";

                // VARCHAR / CHAR length
                meta.length = static_cast<int>(f.length);

                // DECIMAL(p,s): length == precision, decimals == scale
                meta.precision = (meta.sqlType == SqlFieldType::Decimal)
                                     ? static_cast<int>(f.length)
                                     : 0;

                meta.scale = (meta.sqlType == SqlFieldType::Decimal)
                                 ? static_cast<int>(f.decimals)
                                 : 0;
            }
        }

        if (fieldArray_ && fieldsNumber_ > 0)
        {
            fieldsMapPtr_ = std::make_shared<
                std::unordered_map<std::string, RowSizeType>>();
            for (RowSizeType i = 0; i < fieldsNumber_; ++i)
            {
                std::string fieldName = fieldArray_[i].name;
                std::transform(fieldName.begin(),
                               fieldName.end(),
                               fieldName.begin(),
                               [](unsigned char c) { return tolower(c); });
                (*fieldsMapPtr_)[fieldName] = i;
            }
        }

        if (size() > 0)
        {
            rowsPtr_ = std::make_shared<
                std::vector<std::pair<char **, std::vector<unsigned long>>>>();
            MYSQL_ROW row;
            std::vector<unsigned long> vLens;
            vLens.resize(fieldsNumber_);
            while ((row = mysql_fetch_row(result_.get())) != NULL)
            {
                auto lengths = mysql_fetch_lengths(result_.get());
                memcpy(vLens.data(),
                       lengths,
                       sizeof(unsigned long) * fieldsNumber_);
                rowsPtr_->push_back(std::make_pair(row, vLens));
            }
        }
    }

    SizeType size() const noexcept override;
    RowSizeType columns() const noexcept override;
    const char *columnName(RowSizeType number) const override;
    SizeType affectedRows() const noexcept override;
    RowSizeType columnNumber(const char colName[]) const override;
    const char *getValue(SizeType row, RowSizeType column) const override;
    bool isNull(SizeType row, RowSizeType column) const override;
    FieldSizeType getLength(SizeType row, RowSizeType column) const override;
    unsigned long long insertId() const noexcept override;
    const MysqlColumnMeta &columnMeta(SizeType column) const override;

  private:
    const std::shared_ptr<MYSQL_RES> result_;
    const Result::SizeType rowsNumber_;
    const MYSQL_FIELD *fieldArray_;
    std::vector<MysqlColumnMeta> columnMeta_;
    const Result::RowSizeType fieldsNumber_;
    const SizeType affectedRows_;
    const unsigned long long insertId_;
    std::shared_ptr<std::unordered_map<std::string, RowSizeType>> fieldsMapPtr_;
    std::shared_ptr<std::vector<std::pair<char **, std::vector<unsigned long>>>>
        rowsPtr_;
};

}  // namespace orm
}  // namespace drogon
