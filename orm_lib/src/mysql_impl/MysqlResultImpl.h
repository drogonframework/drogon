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

inline SqlType mysqlTypeToSql(enum enum_field_types type,
                              unsigned int flags,
                              unsigned int length)
{
    switch (type)
    {
        case MYSQL_TYPE_TINY:
            // MySQL represents BOOL/BOOLEAN and TINYINT(1) as MYSQL_TYPE_TINY.
            // The client protocol does not preserve the original declaration,
            // so length == 1 is treated as Bool by convention.
            return length == 1 ? SqlType::Bool : SqlType::Int;

        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_LONGLONG:
            return SqlType::Int;

        case MYSQL_TYPE_BIT:
            // BIT(1) is commonly used as a boolean value.
            // BIT(n > 1) remains a Bit type.
            return length == 1 ? SqlType::Bool : SqlType::Bit;

        case MYSQL_TYPE_FLOAT:
            return SqlType::Float;

        case MYSQL_TYPE_DOUBLE:
            return SqlType::Double;

        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return SqlType::Decimal;

        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
            return (flags & BINARY_FLAG) ? SqlType::Binary : SqlType::String;

        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
            return (flags & BINARY_FLAG) ? SqlType::Binary : SqlType::String;

        case MYSQL_TYPE_DATE:
            return SqlType::Date;

        case MYSQL_TYPE_TIME:
#ifdef MYSQL_TYPE_TIME2
        case MYSQL_TYPE_TIME2:
#endif
            return SqlType::Time;

        case MYSQL_TYPE_YEAR:
            return SqlType::Year;

        case MYSQL_TYPE_DATETIME:
#ifdef MYSQL_TYPE_DATETIME2
        case MYSQL_TYPE_DATETIME2:
#endif
            return SqlType::DateTime;

        case MYSQL_TYPE_TIMESTAMP:
#ifdef MYSQL_TYPE_TIMESTAMP2
        case MYSQL_TYPE_TIMESTAMP2:
#endif
            return SqlType::Timestamp;

        case MYSQL_TYPE_JSON:
            return SqlType::Json;

        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
            // ENUM/SET are represented as strings at the logical layer.
            // The exact MySQL type is preserved by mysqlFieldTypeToName().
            return SqlType::String;

        case MYSQL_TYPE_GEOMETRY:
            return SqlType::Geometry;

        default:
            return SqlType::Unknown;
    }
}

inline const char *mysqlFieldTypeToName(enum enum_field_types type,
                                        unsigned int flags)
{
    switch (type)
    {
        case MYSQL_TYPE_TINY:
            return "TINYINT";

        case MYSQL_TYPE_SHORT:
            return "SMALLINT";

        case MYSQL_TYPE_INT24:
            return "MEDIUMINT";

        case MYSQL_TYPE_LONG:
            return "INT";

        case MYSQL_TYPE_LONGLONG:
            return "BIGINT";

        case MYSQL_TYPE_BIT:
            return "BIT";

        case MYSQL_TYPE_FLOAT:
            return "FLOAT";

        case MYSQL_TYPE_DOUBLE:
            return "DOUBLE";

        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return "DECIMAL";

        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
            return (flags & BINARY_FLAG) ? "VARBINARY" : "VARCHAR";

        case MYSQL_TYPE_STRING:
            return (flags & BINARY_FLAG) ? "BINARY" : "CHAR";

        case MYSQL_TYPE_TINY_BLOB:
            return (flags & BINARY_FLAG) ? "TINYBLOB" : "TINYTEXT";

        case MYSQL_TYPE_BLOB:
            return (flags & BINARY_FLAG) ? "BLOB" : "TEXT";

        case MYSQL_TYPE_MEDIUM_BLOB:
            return (flags & BINARY_FLAG) ? "MEDIUMBLOB" : "MEDIUMTEXT";

        case MYSQL_TYPE_LONG_BLOB:
            return (flags & BINARY_FLAG) ? "LONGBLOB" : "LONGTEXT";

        case MYSQL_TYPE_DATE:
            return "DATE";

        case MYSQL_TYPE_TIME:
#ifdef MYSQL_TYPE_TIME2
        case MYSQL_TYPE_TIME2:
#endif
            return "TIME";

        case MYSQL_TYPE_YEAR:
            return "YEAR";

        case MYSQL_TYPE_DATETIME:
#ifdef MYSQL_TYPE_DATETIME2
        case MYSQL_TYPE_DATETIME2:
#endif
            return "DATETIME";

        case MYSQL_TYPE_TIMESTAMP:
#ifdef MYSQL_TYPE_TIMESTAMP2
        case MYSQL_TYPE_TIMESTAMP2:
#endif
            return "TIMESTAMP";

        case MYSQL_TYPE_JSON:
            return "JSON";

        case MYSQL_TYPE_ENUM:
            return "ENUM";

        case MYSQL_TYPE_SET:
            return "SET";

        case MYSQL_TYPE_GEOMETRY:
            return "GEOMETRY";

        case MYSQL_TYPE_NULL:
            return "NULL";

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

            fieldsMapPtr_ = std::make_shared<
                std::unordered_map<std::string, RowSizeType>>();
            fieldsMapPtr_->reserve(fieldsNumber_);

            for (RowSizeType i = 0; i < fieldsNumber_; ++i)
            {
                const MYSQL_FIELD &f = fieldArray_[i];
                auto &meta = columnMeta_[i];
                meta.type = mysqlTypeToSql(f.type, f.flags, f.length);

                // Store native type name
                const char *typeName = mysqlFieldTypeToName(f.type, f.flags);
                meta.nativeType = typeName ? typeName : "UNKNOWN";

                // Extract type attributes based on native type
                meta.nullable = !(f.flags & NOT_NULL_FLAG);
                meta.unsigned_ = (f.flags & UNSIGNED_FLAG) != 0;

                // MYSQL_FIELD::length is not a universal unit. For strings
                // and blobs it is the driver's maximum byte width; for BIT
                // it is the declared number of bits.
                if (f.type == MYSQL_TYPE_VARCHAR ||
                    f.type == MYSQL_TYPE_VAR_STRING ||
                    f.type == MYSQL_TYPE_STRING || f.type == MYSQL_TYPE_BLOB ||
                    f.type == MYSQL_TYPE_TINY_BLOB ||
                    f.type == MYSQL_TYPE_MEDIUM_BLOB ||
                    f.type == MYSQL_TYPE_LONG_BLOB || f.type == MYSQL_TYPE_BIT)
                {
                    meta.length = static_cast<int64_t>(f.length);
                }

                // DECIMAL metadata is represented in the MySQL result metadata
                // as display width and scale. MYSQL_FIELD::length includes
                // formatting overhead such as the sign position for signed
                // values and the decimal point when scale is non-zero.
                //
                // Therefore, precision is inferred by removing these formatting
                // characters from the display width. The scale is taken
                // directly from MYSQL_FIELD::decimals.
                //
                // Note: For result expressions or derived columns, this
                // represents the precision inferred from result metadata and
                // may not correspond to an explicitly declared DECIMAL(M,D)
                // schema definition.
                if (f.type == MYSQL_TYPE_DECIMAL ||
                    f.type == MYSQL_TYPE_NEWDECIMAL)
                {
                    meta.scale = static_cast<int>(f.decimals);
                    const unsigned int displayWidth = f.length;
                    const unsigned int signWidth =
                        (f.flags & UNSIGNED_FLAG) ? 0U : 1U;
                    const unsigned int decimalPointWidth =
                        f.decimals > 0 ? 1U : 0U;
                    const unsigned int overhead = signWidth + decimalPointWidth;
                    if (displayWidth >= overhead + f.decimals)
                    {
                        meta.precision =
                            static_cast<int>(displayWidth - overhead);
                    }
                }

                std::string fieldName = f.name;
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
    const ColumnMeta &columnMeta(SizeType column) const override;

  private:
    const std::shared_ptr<MYSQL_RES> result_;
    const Result::SizeType rowsNumber_;
    const MYSQL_FIELD *fieldArray_;
    std::vector<ColumnMeta> columnMeta_;
    const Result::RowSizeType fieldsNumber_;
    const SizeType affectedRows_;
    const unsigned long long insertId_;
    std::shared_ptr<std::unordered_map<std::string, RowSizeType>> fieldsMapPtr_;
    std::shared_ptr<std::vector<std::pair<char **, std::vector<unsigned long>>>>
        rowsPtr_;
};

}  // namespace orm
}  // namespace drogon
