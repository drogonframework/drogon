/**
 *
 *  @file QueryBuilder.h
 *  @author Ken Matsui
 *
 *  Copyright 2022, Ken Matsui.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/orm/FilterBuilder.h>
#include <string>

namespace drogon
{
namespace orm
{
template <typename T>
class QueryBuilder : public FilterBuilder<T, true>
{
    /**
     * @brief When a user does not set the table name explicitly, then retrieve
     * it from model `T`.
     *
     * @return std::string The table name
     */
    inline const std::string& getTableName() const
    {
        return this->from_.empty() ? T::tableName : this->from_;
    }

  public:
    /**
     * @brief Set from which table to return.
     *
     * @param table The table.
     *
     * @return QueryBuilder& The QueryBuilder itself.
     */
    inline QueryBuilder& from(const std::string& table)
    {
        this->from_ = table;
        return *this;
    }

    /**
     * @brief Select specific columns.
     *
     * @param columns The columns.
     *
     * @return FilterBuilder<T, false> A new FilterBuilder.
     *
     * @note If you would return all rows, please use the `selectAll` method.
     * The method can return rows as model type `T`.
     */
    inline FilterBuilder<T, false> select(const std::string& columns) const
    {
        return {getTableName(), columns};
    }

    /**
     * @brief Select all columns.
     *
     * @return FilterBuilder<T, true> A new FilterBuilder.
     */
    inline FilterBuilder<T, true> selectAll() const
    {
        return {getTableName(), "*"};
    }
};
}  // namespace orm
}  // namespace drogon
