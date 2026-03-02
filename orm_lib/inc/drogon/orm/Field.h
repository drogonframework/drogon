/**
 *
 *  @file Field.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

// Taken from libpqxx and modified.
// The license for libpqxx can be found in the COPYING file.

#pragma once

#include <drogon/exports.h>
#include <string_view>
#include <drogon/orm/ArrayParser.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <trantor/utils/Logger.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifdef __linux__
#include <arpa/inet.h>
#endif

namespace drogon
{
namespace orm
{

/// Reference to a field in a result set.
class DROGON_EXPORT Field
{
  public:
    using SizeType = unsigned long;

    /// Column name
    const char *name() const;

    /// Is this field's value null?
    bool isNull() const;

    /// Raw C-string value
    const char *c_str() const;

    /// Get the length of the plain C string
    size_t length() const
    {
        return result_.getLength(row_, column_);
    }

    // =========================================================
    // 🔥 NEW METADATA APIs
    // =========================================================

    SqlFieldType sqlType() const noexcept { return result_.getSqlType(column_); }

    /// SQL type name (VARCHAR, INT, DECIMAL, etc.)
    const std::string &typeName() const noexcept { return result_.getTypeName(column_); }

    /// Character length (VARCHAR)
    int columnLength() const noexcept { return result_.getColumnLength(column_); }

    /// Numeric precision (DECIMAL / NUMERIC)
    int precision() const noexcept { return result_.getPrecision(column_); }

    /// Numeric scale (DECIMAL / NUMERIC)
    int scale() const noexcept { return result_.getScale(column_); }

    /// Convert to a type T value
    template <typename T>
    T as() const
    {
        if (isNull())
            return T();

        auto data_ = result_.getValue(row_, column_);
        T value{};
        if (data_)
        {
            try
            {
                std::stringstream ss(data_);
                ss >> value;
            }
            catch (...)
            {
                LOG_DEBUG << "Type error";
            }
        }
        return value;
    }

    ArrayParser getArrayParser() const
    {
        return ArrayParser(result_.getValue(row_, column_));
    }

    template <typename T>
    std::vector<std::shared_ptr<T>> asArray() const
    {
        std::vector<std::shared_ptr<T>> ret;
        auto parser = getArrayParser();

        while (true)
        {
            auto val = parser.getNext();
            if (val.first == ArrayParser::juncture::done)
                break;

            if (val.first == ArrayParser::juncture::string_value)
            {
                T v{};
                std::stringstream ss(std::move(val.second));
                ss >> v;
                ret.emplace_back(std::make_shared<T>(v));
            }
            else if (val.first == ArrayParser::juncture::null_value)
            {
                ret.emplace_back(nullptr);
            }
        }
        return ret;
    }

  protected:
    Result::SizeType row_;
    long column_;

    friend class Row;
    friend class MysqlResultImpl;
    friend class PgResultImpl;
    friend class Sqlite3ResultImpl;

    Field(const Row &row, Row::SizeType columnNum) noexcept;

  private:
    const Result result_;
};

// =============================================================
// Explicit template specializations (UNCHANGED)
// =============================================================

template <>
DROGON_EXPORT std::string Field::as<std::string>() const;

template <>
DROGON_EXPORT const char *Field::as<const char *>() const;

template <>
DROGON_EXPORT char *Field::as<char *>() const;

template <>
DROGON_EXPORT std::vector<char> Field::as<std::vector<char>>() const;

template <>
inline std::string_view Field::as<std::string_view>() const
{
    auto first = result_.getValue(row_, column_);
    auto len = result_.getLength(row_, column_);
    return {first, len};
}

template <>
inline float Field::as<float>() const
{
    return isNull() ? 0.0f : std::stof(result_.getValue(row_, column_));
}

template <>
inline double Field::as<double>() const
{
    return isNull() ? 0.0 : std::stod(result_.getValue(row_, column_));
}

template <>
inline bool Field::as<bool>() const
{
    if (result_.getLength(row_, column_) != 1)
        return false;
    char v = *result_.getValue(row_, column_);
    return (v == 't' || v == '1');
}

template <>
inline int Field::as<int>() const
{
    return isNull() ? 0 : std::stoi(result_.getValue(row_, column_));
}

template <>
inline long Field::as<long>() const
{
    return isNull() ? 0L : std::stol(result_.getValue(row_, column_));
}

template <>
inline long long Field::as<long long>() const
{
    return isNull() ? 0LL : std::stoll(result_.getValue(row_, column_));
}

template <>
inline unsigned long long Field::as<unsigned long long>() const
{
    return isNull() ? 0ULL : std::stoull(result_.getValue(row_, column_));
}

}  // namespace orm
}  // namespace drogon
