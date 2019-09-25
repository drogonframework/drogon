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
    _condString = json[0].asString();
    if (!json[2].isNull() && !json[2].isArray())
    {
        if (json[1].asString() == "in")
        {
            throw std::runtime_error("Json format error");
        }
        _condString.append(json[1].asString());
        _condString.append("$?");
        _outputArgumentsFunc =
            [arg = json[2].asString()](internal::SqlBinder &binder) {
                binder << arg;
            };
    }
    else if (json[2].isNull())
    {
        if (json[1].asString() == "=")
        {
            _condString.append(" is null");
        }
        else if (json[1].asString() == "!=")
        {
            _condString.append(" is not null");
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
        _condString.append(" in (");
        for (size_t i = 0; i < json[2].size(); ++i)
        {
            if (i < json[2].size() - 1)
            {
                _condString.append("$?,");
            }
            else
            {
                _condString.append("$?");
            }
        }
        _condString.append(1, ')');
        _outputArgumentsFunc = [args = json[2]](internal::SqlBinder &binder) {
            for (auto &arg : args)
            {
                binder << arg.asString();
            }
        };
    }
}

}  // namespace orm
}  // namespace drogon
