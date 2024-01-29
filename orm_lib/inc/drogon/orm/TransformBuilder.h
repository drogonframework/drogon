/**
 *
 *  @file TransformBuilder.h
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

#include <drogon/orm/BaseBuilder.h>
#include <string>
#include <type_traits>

namespace drogon
{
namespace orm
{
template <typename T, bool SelectAll, bool Single>
class TransformBuilder : public BaseBuilder<T, SelectAll, Single>
{
  public:
    /**
     * @brief A default constructor for derived classes.
     *
     * @return TransformBuilder The TransformBuilder itself.
     */
    TransformBuilder() = default;

    /**
     * @brief A copy constructor from a non `Single` builder to `Single`
     * builder, used by the `single` method.
     *
     * @return TransformBuilder The TransformBuilder itself.
     *
     * @note This function is enabled only when `Single` is true.
     */
    template <bool SI = Single, std::enable_if_t<SI, std::nullptr_t> = nullptr>
    TransformBuilder(const TransformBuilder<T, SelectAll, false> &tb)
    {
        this->from_ = tb.from_;
        this->columns_ = tb.columns_;
        this->filters_ = tb.filters_;
        this->limit_ = tb.limit_;
        this->offset_ = tb.offset_;
        this->orders_ = tb.orders_;
    }

    /**
     * @brief Limit the result to `count`.
     *
     * @param count The number of rows to be limited.
     *
     * @return TransformBuilder& The TransformBuilder itself.
     */
    inline TransformBuilder &limit(std::uint64_t count)
    {
        this->limit_ = count;
        return *this;
    }

    /**
     * @brief Add a offset to the query.
     *
     * @param offset The offset.
     *
     * @return TransformBuilder& The TransformBuilder itself.
     */
    inline TransformBuilder &offset(std::uint64_t count)
    {
        this->offset_ = count;
        return *this;
    }

    /**
     * @brief Limit the result to an inclusive range.
     *
     * @param from The first index to limit the result.
     * @param to The last index to limit the result.
     *
     * @return TransformBuilder& The TransformBuilder itself.
     */
    inline TransformBuilder &range(std::uint64_t from, std::uint64_t to)
    {
        this->offset_ = from;
        this->limit_ = to - from + 1;  // inclusive
        return *this;
    }

    /**
     * @brief Order the result.
     *
     * @param column The column to order by.
     * @param asc If `true`, ascending order. If `false`, descending order.
     *
     * @return TransformBuilder& The TransformBuilder itself.
     */
    inline TransformBuilder &order(const std::string &column, bool asc = true)
    {
        this->assert_column(column);
        this->orders_.emplace_back(column, asc);
        return *this;
    }

    /**
     * @brief Ensure returning only one row.
     *
     * @return TransformBuilder<T, SelectAll, true> The TransformBuilder where
     * Single is true and all else is the same.
     *
     * @note This function can be called only once throughout an instance of a
     * builder.
     */
    template <bool SI = Single, std::enable_if_t<!SI, std::nullptr_t> = nullptr>
    inline TransformBuilder<T, SelectAll, true> single() const
    {
        return {*this};
    }
};
}  // namespace orm
}  // namespace drogon
