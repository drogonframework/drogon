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
class Criteria
{
  public:
    explicit operator bool() const
    {
        return !_condString.empty();
    }
    std::string criteriaString() const
    {
        return _condString;
    }
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

    template <typename T>
    Criteria(const std::string &colName, T &&arg)
        : Criteria(colName, CompareOperator::EQ, arg)
    {
    }

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
    Criteria()
    {
    }
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
