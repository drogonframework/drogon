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
            if (!andJson[0].isString())
            {
                throw std::runtime_error("Json format error");
            }
            auto index = columnIndexForField(andJson[0].asString());
            if (!index)
            {
                throw std::runtime_error("Json format error");
            }
            Json::Value newJson(andJson);
            newJson[0] = columnsVector_[*index];
            if (!orCriteria)
            {
                orCriteria = orm::Criteria(newJson);
            }
            else
            {
                orCriteria = orCriteria && orm::Criteria(newJson);
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
