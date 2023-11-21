/**
 *
 *  @file BaseBuilder.h
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

#include <drogon/orm/Criteria.h>
#include <drogon/orm/DbClient.h>
#include <string_view>
#include <future>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <optional>

#define unimplemented() assert(false && "unimplemented")

namespace drogon
{
namespace orm
{
inline std::string to_string(CompareOperator op)
{
    switch (op)
    {
        case CompareOperator::EQ:
            return "=";
        case CompareOperator::NE:
            return "!=";
        case CompareOperator::GT:
            return ">";
        case CompareOperator::GE:
            return ">=";
        case CompareOperator::LT:
            return "<";
        case CompareOperator::LE:
            return "<=";
        case CompareOperator::Like:
            return "like";
        case CompareOperator::NotLike:
        case CompareOperator::In:
        case CompareOperator::NotIn:
        case CompareOperator::IsNull:
        case CompareOperator::IsNotNull:
        default:
            unimplemented();
            return "";
    }
}

struct Filter
{
    std::string column;
    CompareOperator op;
    std::string value;
};

// Forward declaration to be a friend
template <typename T, bool SelectAll, bool Single = false>
class TransformBuilder;

template <typename T, bool SelectAll, bool Single = false>
class BaseBuilder
{
    using ResultType =
        std::conditional_t<SelectAll,
                           std::conditional_t<Single, T, std::vector<T>>,
                           std::conditional_t<Single, Row, Result>>;

    // Make the constructor of `TransformBuilder<T, SelectAll, true>` through
    // `TransformBuilder::single()` be able to read these protected members.
    friend class TransformBuilder<T, SelectAll, true>;

  protected:
    std::string from_;
    std::string columns_;
    std::vector<Filter> filters_;
    std::optional<std::uint64_t> limit_;
    std::optional<std::uint64_t> offset_;
    // The order is important; use vector<pair> instead of unordered_map and
    // map.
    std::vector<std::pair<std::string, bool>> orders_;

    inline void assert_column(const std::string &colName) const
    {
        for (const typename T::MetaData &m : T::metaData_)
        {
            if (m.colName_ == colName)
            {
                return;
            }
        }
        throw UsageError("The column `" + colName +
                         "` is not in the specified table.");
    }

  private:
    /**
     * @brief Generate SQL query in string.
     *
     * @return std::string The string generated SQL query.
     */
    inline std::string gen_sql(ClientType type) const noexcept
    {
        int pCount = 0;
        const auto placeholder = [type, &pCount]() {
            ++pCount;
            return type == ClientType::PostgreSQL ? "$" + std::to_string(pCount)
                                                  : "?";
        };

        std::string sql = "select " + columns_ + " from " + from_;
        if (!filters_.empty())
        {
            sql += " where " + filters_[0].column + " " +
                   to_string(filters_[0].op) + " " + placeholder() + "";
            for (int i = 1; i < filters_.size(); ++i)
            {
                sql += " and " + filters_[i].column + " " +
                       to_string(filters_[i].op) + " " + placeholder() + "";
            }
        }
        if (!orders_.empty())
        {
            sql += " order by " + orders_[0].first + " " +
                   std::string(orders_[0].second ? "asc" : "desc");
            for (int i = 1; i < orders_.size(); ++i)
            {
                sql += ", " + orders_[i].first + " " +
                       std::string(orders_[i].second ? "asc" : "desc");
            }
        }
        if (limit_.has_value())
        {
            sql += " limit " + std::to_string(limit_.value());
        }
        if (offset_.has_value())
        {
            sql += " offset " + std::to_string(offset_.value());
        }
        return sql;
    }

    inline std::vector<std::string> gen_args() const noexcept
    {
        std::vector<std::string> args;
        if (!filters_.empty())
        {
            for (const Filter &f : filters_)
            {
                args.emplace_back(f.value);
            }
        }
        return args;
    }

  public:
    static ResultType convert_result(const Result &r)
    {
        if constexpr (SelectAll)
        {
            if constexpr (Single)
            {
                return T(r[0]);
            }
            else
            {
                std::vector<T> ret;
                for (const Row &row : r)
                {
                    ret.emplace_back(T(row));
                }
                return ret;
            }
        }
        else
        {
            if constexpr (Single)
            {
                return r[0];
            }
            else
            {
                return r;
            }
        }
    }

    inline ResultType execSync(const DbClientPtr &client)
    {
        Result r(nullptr);
        {
            auto binder = *client << gen_sql(client->type());
            for (const std::string &a : gen_args())
            {
                binder << a;
            }
            binder << Mode::Blocking;
            binder >> [&r](const Result &result) { r = result; };
            binder.exec();  // exec may throw exception
        }
        return convert_result(r);
    }

    template <typename TFn, typename EFn>
    void execAsync(const DbClientPtr &client,
                   TFn &&rCallback,
                   EFn &&exceptCallback) noexcept
    {
        auto binder = *client << gen_sql(client->type());
        for (const std::string &a : gen_args())
        {
            binder << a;
        }
        binder >> std::forward<TFn>(rCallback);
        binder >> std::forward<EFn>(exceptCallback);
    }

    inline std::future<ResultType> execAsyncFuture(
        const DbClientPtr &client) noexcept
    {
        auto binder = *client << gen_sql(client->type());
        for (const std::string &a : gen_args())
        {
            binder << a;
        }
        std::shared_ptr<std::promise<ResultType>> prom =
            std::make_shared<std::promise<ResultType>>();
        binder >>
            [prom](const Result &r) { prom->set_value(convert_result(r)); };
        binder >>
            [prom](const std::exception_ptr &e) { prom->set_exception(e); };
        binder.exec();
        return prom->get_future();
    }
};
}  // namespace orm
}  // namespace drogon
