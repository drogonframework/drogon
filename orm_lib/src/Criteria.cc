/**
 *
 *  Criteria.cc
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

#include <drogon/orm/Criteria.h>

namespace drogon
{
namespace orm
{

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
