/**
 *
 *  @file Criteria.h
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

#pragma once

#include <drogon/exports.h>
#include <drogon/orm/SqlBinder.h>
#include <drogon/utils/apply.h>
#include <assert.h>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

namespace Json
{
class Value;
}
namespace drogon
{
namespace orm
{
enum class CompareOperator
{
    EQ,
    NE,
    GT,
    GE,
    LT,
    LE,
    Like,
    NotLike,
    In,
    NotIn,
    IsNull,
    IsNotNull
};

/**
 * @brief Wrapper for custom SQL string
 */
struct CustomSql
{
    explicit CustomSql(std::string content) : content_(std::move(content))
    {
    }
    std::string content_;
};

/**
 * @brief User-defined literal to convert a string to CustomSql
 */
inline CustomSql operator""_sql(const char *str, size_t)
{
    return CustomSql(str);
}

/**
 * @brief this class represents a comparison condition.
 */
class DROGON_EXPORT Criteria
{
  public:
    /**
     * @brief Check if the condition is empty.
     *
     * @return true means the condition is not empty.
     * @return false means the condition is empty.
     */
    explicit operator bool() const
    {
        return !conditionString_.empty();
    }

    /**
     * @brief return the condition string in SQL.
     *
     * @return std::string
     */
    std::string criteriaString() const
    {
        return conditionString_;
    }

    /**
     * @brief Construct a new custom Criteria object
     *
     * @param sql The SQL statement to be executed.
     * @param args The parameters to be passed to the placeholders.
     *
     * @note
     *
     * Use $? as placeholders in the SQL statement.
     *
     * Be careful that the placeholders are not the same as the ones in
     * functions such as drogon::orm::DbClient::execSqlAsync. Which means you
     * couldn't use numeric placeholders such as $1, $2 to represent the order
     * of the arguments.
     *
     * The arguments should be in the same order as the placeholders.
     *
     */
    template <typename... Arguments>
    explicit Criteria(const CustomSql &sql, Arguments &&...args)
    {
        conditionString_ = sql.content_;
        outputArgumentsFunc_ =
            [args = std::make_tuple(std::forward<Arguments>(args)...)](
                internal::SqlBinder &binder) mutable {
                return apply(
                    [&binder](auto &&...args) {
                        (void)std::initializer_list<int>{
                            (binder << std::forward<Arguments>(args), 0)...};
                    },
                    std::move(args));
            };
    }

    template <typename... Arguments>
    explicit Criteria(CustomSql &&sql, Arguments &&...args)
    {
        conditionString_ = std::move(sql.content_);
        outputArgumentsFunc_ =
            [args = std::make_tuple(std::forward<Arguments>(args)...)](
                internal::SqlBinder &binder) mutable {
                return apply(
                    [&binder](auto &&...args) {
                        (void)std::initializer_list<int>{
                            (binder << std::forward<Arguments>(args), 0)...};
                    },
                    std::move(args));
            };
    }

    /**
     * @brief Construct a new Criteria object
     *
     * @tparam T the type of the object to be compared with.
     * @param colName The name of the column.
     * @param opera The type of the comparison.
     * @param arg The object to be compared with.
     */
    template <typename T>
    Criteria(const std::string &colName, const CompareOperator &opera, T &&arg)
    {
        assert(opera != CompareOperator::IsNotNull &&
               opera != CompareOperator::IsNull);
        conditionString_ = colName;
        switch (opera)
        {
            case CompareOperator::EQ:
                conditionString_ += " = $?";
                break;
            case CompareOperator::NE:
                conditionString_ += " != $?";
                break;
            case CompareOperator::GT:
                conditionString_ += " > $?";
                break;
            case CompareOperator::GE:
                conditionString_ += " >= $?";
                break;
            case CompareOperator::LT:
                conditionString_ += " < $?";
                break;
            case CompareOperator::LE:
                conditionString_ += " <= $?";
                break;
            case CompareOperator::Like:
                conditionString_ += " like $?";
                break;
            case CompareOperator::NotLike:
                conditionString_ += " not like $?";
                break;
            default:
                break;
        }
        outputArgumentsFunc_ =
            [arg = std::forward<T>(arg)](internal::SqlBinder &binder) {
                binder << arg;
            };
    }

    template <typename T>
    Criteria(const std::string &colName,
             const CompareOperator &opera,
             const std::vector<T> &args)
    {
        const auto argsSize = args.size();
        assert(((opera == CompareOperator::In) ||
                (opera == CompareOperator::NotIn)) &&
               (argsSize > 0));
        if (opera == CompareOperator::In)
        {
            conditionString_ = colName + " in (";
        }
        else if (opera == CompareOperator::NotIn)
        {
            conditionString_ = colName + " not in (";
        }
        for (size_t i = 0; i < argsSize; ++i)
        {
            if (i < (argsSize - 1))
                conditionString_.append("$?,");
            else
                conditionString_.append("$?");
        }
        conditionString_.append(")");
        outputArgumentsFunc_ = [args](internal::SqlBinder &binder) {
            for (auto &arg : args)
            {
                binder << arg;
            }
        };
    }

    template <typename T>
    Criteria(const std::string &colName,
             const CompareOperator &opera,
             std::vector<T> &&args)
    {
        const auto argsSize = args.size();
        assert(((opera == CompareOperator::In) ||
                (opera == CompareOperator::NotIn)) &&
               (argsSize > 0));
        if (opera == CompareOperator::In)
        {
            conditionString_ = colName + " in (";
        }
        else if (opera == CompareOperator::NotIn)
        {
            conditionString_ = colName + " not in (";
        }
        for (size_t i = 0; i < argsSize; ++i)
        {
            if (i < (argsSize - 1))
                conditionString_.append("$?,");
            else
                conditionString_.append("$?");
        }
        conditionString_.append(")");
        outputArgumentsFunc_ =
            [args = std::move(args)](internal::SqlBinder &binder) {
                for (auto &arg : args)
                {
                    binder << arg;
                }
            };
    }

    template <typename T>
    Criteria(const std::string &colName,
             const CompareOperator &opera,
             std::vector<T> &args)
        : Criteria(colName, opera, (const std::vector<T> &)args)
    {
    }

    /**
     * @brief Construct a new Criteria object presenting a equal expression
     *
     * @tparam T The type of the object to be equal to.
     * @param colName The name of the column.
     * @param arg The object to be equal to.
     */
    template <typename T>
    Criteria(const std::string &colName, T &&arg)
        : Criteria(colName, CompareOperator::EQ, std::forward<T>(arg))
    {
    }

    /**
     * @brief Construct a new Criteria object presenting a expression without
     * parameters.
     *
     * @param colName The name of the column.
     * @param opera The type of the comparison. it must be one of the following:
     * @code
       CompareOperator::IsNotNull
       CompareOperator::IsNull
       @endcode
     */
    Criteria(const std::string &colName, const CompareOperator &opera)
    {
        assert(opera == CompareOperator::IsNotNull ||
               opera == CompareOperator::IsNull);
        conditionString_ = colName;
        switch (opera)
        {
            case CompareOperator::IsNull:
                conditionString_ += " is null";
                break;
            case CompareOperator::IsNotNull:
                conditionString_ += " is not null";
                break;
            default:
                break;
        }
    }
    Criteria(const std::string &colName, CompareOperator &opera)
        : Criteria(colName, (const CompareOperator &)opera)
    {
    }
    Criteria(const std::string &colName, CompareOperator &&opera)
        : Criteria(colName, (const CompareOperator &)opera)
    {
    }

    /**
     * @brief Construct a new Criteria object
     *
     * @param json A json object representing the criteria
     * @note the json object must be a array of three items, the first is the
     * name of the field, the second is the comparison operator and the third is
     * the value to be compared.
     * The valid operators are "=","<",">","<=",">=","!=","in"
     * for example:
     * ["id","=",1] means 'id = 1'
     * ["id","!=",null] means 'id is not null'
     * ["user_name","in",["Tom","Bob"]] means 'user_name in ('Tom', 'Bob')'
     * ["price","<",1000] means 'price < 1000'
     */
    explicit Criteria(const Json::Value &json) noexcept(false);
    Criteria() = default;

    /**
     * @brief Output arguments to the SQL binder object.
     *
     * @param binder The SQL binder object.
     * @note This method must be called by drogon.
     */
    void outputArgs(internal::SqlBinder &binder) const
    {
        if (outputArgumentsFunc_)
            outputArgumentsFunc_(binder);
    }

  private:
    friend DROGON_EXPORT const Criteria operator&&(Criteria cond1,
                                                   Criteria cond2);

    friend DROGON_EXPORT const Criteria operator||(Criteria cond1,
                                                   Criteria cond2);
    std::string conditionString_;
    std::function<void(internal::SqlBinder &)> outputArgumentsFunc_;
};  // namespace orm

DROGON_EXPORT const Criteria operator&&(Criteria cond1, Criteria cond2);
DROGON_EXPORT const Criteria operator||(Criteria cond1, Criteria cond2);

}  // namespace orm
}  // namespace drogon
