/**
 *
 *  RestfulController.cc
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

#include <drogon/orm/RestfulController.h>

using namespace drogon;

orm::Criteria RestfulController::makeCriteria(
    const Json::Value &pJson) noexcept(false)
{
    if (!pJson.isArray())
    {
        throw std::runtime_error("Json format error");
    }
    orm::Criteria ret;
    for (auto &orJson : pJson)
    {
        if (!orJson.isArray())
        {
            throw std::runtime_error("Json format error");
        }
        orm::Criteria orCriteria;
        for (auto &andJson : orJson)
        {
            if (!andJson.isArray() || andJson.size() != 3)
            {
                throw std::runtime_error("Json format error");
            }
            if (masquerading_)
            {
                Json::Value newJson(andJson);
                auto iter = masqueradingMap_.find(newJson[0].asString());
                if (iter != masqueradingMap_.end())
                {
                    newJson[0] = masqueradingVector_[iter->second];
                    if (!orCriteria)
                    {
                        orCriteria = orm::Criteria(newJson);
                    }
                    else
                    {
                        orCriteria = orCriteria && orm::Criteria(newJson);
                    }
                }
                else
                {
                    throw std::runtime_error("Json format error");
                }
            }
            else
            {
                if (!orCriteria)
                {
                    orCriteria = orm::Criteria(andJson);
                }
                else
                {
                    orCriteria = orCriteria && orm::Criteria(andJson);
                }
            }
        }
        if (!ret)
        {
            ret = std::move(orCriteria);
        }
        else
        {
            ret = ret || orCriteria;
        }
    }
    return ret;
}