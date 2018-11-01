/**
 *
 *  Criteria.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#include <drogon/config.h>
#include <drogon/orm/SqlBinder.h>

#include <string>
#include <tuple>
#include <memory>
#include <type_traits>
#include <assert.h>

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
    IsNull,
    IsNotNull
};
class Criteria
{
  public:
    explicit operator bool() const { return !_condString.empty(); }
    std::string criteriaString() const { return _condString; }
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
        case CompareOperator::IsNull:
        case CompareOperator::IsNotNull:
        default:
            break;
        }
        _outputArgumentsFunc = [=](internal::SqlBinder &binder) {
            binder << arg;
        };
    }

    template <typename M, typename T>
    Criteria(const typename M::Col idx, const CompareOperator &opera, T &&arg)
        : Criteria(M::getColumnName((int)idx), opera, arg)
    {
    }

    template <typename M>
    Criteria(const typename M::Col idx, const CompareOperator &opera)
        : Criteria(M::getColumnName((int)idx), opera)
    {
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
    Criteria() {}
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
}; // namespace orm

const Criteria operator&&(Criteria cond1, Criteria cond2)
{
    assert(cond1);
    assert(cond2);
    Criteria cond;
    cond._condString = "( ";
    cond._condString += cond1._condString;
    cond._condString += " ) and ( ";
    cond._condString += cond2._condString;
    cond._condString += " )";
    auto cond1Ptr = std::make_shared<Criteria>(std::move(cond1));
    auto cond2Ptr = std::make_shared<Criteria>(std::move(cond2));
    cond._outputArgumentsFunc = [=](internal::SqlBinder &binder) {
        if (cond1Ptr->_outputArgumentsFunc)
        {
            cond1Ptr->_outputArgumentsFunc(binder);
        }
        if (cond2Ptr->_outputArgumentsFunc)
        {
            cond2Ptr->_outputArgumentsFunc(binder);
        }
    };
    return cond;
}
const Criteria operator||(Criteria cond1, Criteria cond2)
{
    assert(cond1);
    assert(cond2);
    Criteria cond;
    cond._condString = "( ";
    cond._condString += cond1._condString;
    cond._condString += " ) or ( ";
    cond._condString += cond2._condString;
    cond._condString += " )";
    auto cond1Ptr = std::make_shared<Criteria>(std::move(cond1));
    auto cond2Ptr = std::make_shared<Criteria>(std::move(cond2));
    cond._outputArgumentsFunc = [=](internal::SqlBinder &binder) {
        if (cond1Ptr->_outputArgumentsFunc)
        {
            cond1Ptr->_outputArgumentsFunc(binder);
        }
        if (cond2Ptr->_outputArgumentsFunc)
        {
            cond2Ptr->_outputArgumentsFunc(binder);
        }
    };
    return cond;
}

} // namespace orm
} // namespace drogon