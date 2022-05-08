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
    IsNotNull,

    /// For postgresql generic
    And,
    Or,
    GG,
    GGE,
    LL,
    LLE,

    /// For postgresql ranges
    Subset,
    Superset,
    Adjacent,

    /// For postgresql patterns
    Similar,
    NotSimilar,

    /// For postgresql search
    Match,

    /// For postgresql jsonb
    IsElem,
    AnyAreElem,
    AllAreElem,
    PathToItem,
};
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
     * @param lValue The string before operand.
     * @param opera The custom operand.
     * @param rValue The string after operand.
     */
    Criteria(const std::string &lValue, const std::string &opera, const std::string &rValue)
    {
        conditionString_ = lValue + " " + opera + " " + rValue;
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
            case CompareOperator::And:
                conditionString_ += " && $?";
                break;
            case CompareOperator::Or:
                conditionString_ += " || $?";
                break;
            case CompareOperator::GG:
                conditionString_ += " >> $?";
                break;
            case CompareOperator::GGE:
                conditionString_ += " &> $?";
                break;
            case CompareOperator::LL:
                conditionString_ += " << $?";
                break;
            case CompareOperator::LLE:
                conditionString_ += " &< $?";
                break;
            case CompareOperator::Subset:
                conditionString_ += " <@ $?";
                break;
            case CompareOperator::Superset:
                conditionString_ += " @> $?";
                break;
            case CompareOperator::Adjacent:
                conditionString_ += " -|- $?";
                break;
            case CompareOperator::Similar:
                conditionString_ += " similar to $?";
                break;
            case CompareOperator::NotSimilar:
                conditionString_ += " not similar to $?";
                break;
            case CompareOperator::Match:
                conditionString_ += " @@ $?";
                break;
            case CompareOperator::IsElem:
                conditionString_ += " ? $?";
                break;
            case CompareOperator::AnyAreElem:
                conditionString_ += " ?| $?";
                break;
            case CompareOperator::AllAreElem:
                conditionString_ += " ?& $?";
                break;
            case CompareOperator::PathToItem:
                conditionString_ += " @? $?";
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
        : Criteria(colName, CompareOperator::EQ, arg)
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
    Criteria(const Json::Value &json) noexcept(false);

    Criteria()
    {
    }

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
