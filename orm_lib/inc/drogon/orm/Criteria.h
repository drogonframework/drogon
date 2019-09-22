/**
 *
 *  Criteria.h
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

#include <drogon/orm/SqlBinder.h>

#include <assert.h>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

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
    LIKE,
    IsNull,
    IsNotNull
};
/**
 * @brief this class represents a comparison condition.
 */
class Criteria
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
        return !_condString.empty();
    }

    /**
     * @brief return the condition string in SQL.
     *
     * @return std::string
     */
    std::string criteriaString() const
    {
        return _condString;
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
        _condString = colName;
        switch (opera)
        {
            case CompareOperator::EQ:
                _condString += " = $?";
                break;
            case CompareOperator::NE:
                _condString += " != $?";
                break;
            case CompareOperator::GT:
                _condString += " > $?";
                break;
            case CompareOperator::GE:
                _condString += " >= $?";
                break;
            case CompareOperator::LT:
                _condString += " < $?";
                break;
            case CompareOperator::LE:
                _condString += " <= $?";
                break;
            case CompareOperator::LIKE:
                _condString += " like $?";
                break;
            case CompareOperator::IsNull:
            case CompareOperator::IsNotNull:
            default:
                break;
        }
        _outputArgumentsFunc = [=](internal::SqlBinder &binder) {
            binder << arg;
        };
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
        _condString = colName;
        switch (opera)
        {
            case CompareOperator::IsNull:
                _condString += " is null";
                break;
            case CompareOperator::IsNotNull:
                _condString += " is not null";
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
        if (_outputArgumentsFunc)
            _outputArgumentsFunc(binder);
    }

  private:
    friend const Criteria operator&&(Criteria cond1, Criteria cond2);
    friend const Criteria operator||(Criteria cond1, Criteria cond2);
    std::string _condString;
    std::function<void(internal::SqlBinder &)> _outputArgumentsFunc;
};  // namespace orm

const Criteria operator&&(Criteria cond1, Criteria cond2);
const Criteria operator||(Criteria cond1, Criteria cond2);

}  // namespace orm
}  // namespace drogon
