/**
 *
 *  @file Criteria.cc
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

#include <drogon/orm/Criteria.h>
#include <json/json.h>

namespace drogon
{
namespace orm
{
const Criteria operator&&(Criteria cond1, Criteria cond2)
{
    assert(cond1);
    assert(cond2);
    Criteria cond;
    cond.conditionString_ = "( ";
    cond.conditionString_ += cond1.conditionString_;
    cond.conditionString_ += " ) and ( ";
    cond.conditionString_ += cond2.conditionString_;
    cond.conditionString_ += " )";
    auto cond1Ptr = std::make_shared<Criteria>(std::move(cond1));
    auto cond2Ptr = std::make_shared<Criteria>(std::move(cond2));
    cond.outputArgumentsFunc_ = [cond1Ptr,
                                 cond2Ptr](internal::SqlBinder &binder) {
        if (cond1Ptr->outputArgumentsFunc_)
        {
            cond1Ptr->outputArgumentsFunc_(binder);
        }
        if (cond2Ptr->outputArgumentsFunc_)
        {
            cond2Ptr->outputArgumentsFunc_(binder);
        }
    };
    return cond;
}
const Criteria operator||(Criteria cond1, Criteria cond2)
{
    assert(cond1);
    assert(cond2);
    Criteria cond;
    cond.conditionString_ = "( ";
    cond.conditionString_ += cond1.conditionString_;
    cond.conditionString_ += " ) or ( ";
    cond.conditionString_ += cond2.conditionString_;
    cond.conditionString_ += " )";
    auto cond1Ptr = std::make_shared<Criteria>(std::move(cond1));
    auto cond2Ptr = std::make_shared<Criteria>(std::move(cond2));
    cond.outputArgumentsFunc_ = [cond1Ptr,
                                 cond2Ptr](internal::SqlBinder &binder) {
        if (cond1Ptr->outputArgumentsFunc_)
        {
            cond1Ptr->outputArgumentsFunc_(binder);
        }
        if (cond2Ptr->outputArgumentsFunc_)
        {
            cond2Ptr->outputArgumentsFunc_(binder);
        }
    };
    return cond;
}

Criteria::Criteria(const Json::Value &json) noexcept(false)
{
    if (!json.isArray() || json.size() != 3)
    {
        throw std::runtime_error("Json format error");
    }
    if (!json[0].isString() || !json[1].isString())
    {
        throw std::runtime_error("Json format error");
    }
    conditionString_ = json[0].asString();
    if (!json[2].isNull() && !json[2].isArray())
    {
        if (json[1].asString() == "in")
        {
            throw std::runtime_error("Json format error");
        }
        conditionString_.append(json[1].asString());
        conditionString_.append("$?");
        outputArgumentsFunc_ =
            [arg = json[2].asString()](internal::SqlBinder &binder) {
                binder << arg;
            };
    }
    else if (json[2].isNull())
    {
        if (json[1].asString() == "=")
        {
            conditionString_.append(" is null");
        }
        else if (json[1].asString() == "!=")
        {
            conditionString_.append(" is not null");
        }
        else
        {
            throw std::runtime_error("Json format error");
        }
    }
    else
    {
        assert(json[2].isArray());
        if (json[1].asString() != "in")
        {
            throw std::runtime_error("Json format error");
        }
        conditionString_.append(" in (");
        for (size_t i = 0; i < json[2].size(); ++i)
        {
            if (i < json[2].size() - 1)
            {
                conditionString_.append("$?,");
            }
            else
            {
                conditionString_.append("$?");
            }
        }
        conditionString_.append(1, ')');
        outputArgumentsFunc_ = [args = json[2]](internal::SqlBinder &binder) {
            for (auto &arg : args)
            {
                binder << arg.asString();
            }
        };
    }
}

}  // namespace orm
}  // namespace drogon
