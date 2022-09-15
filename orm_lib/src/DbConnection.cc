/**
 *
 *  @file DbConnection.cc
 *  @author Martin Chang
 *
 *  Copyright 2020, Martin Chang.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "DbConnection.h"
#include "DbSubscribeContext.h"

#include <regex>

using namespace drogon::orm;

std::map<std::string, std::string> DbConnection::parseConnString(
    const std::string& connInfo)
{
    const static std::regex re(
        R"((\w+) *= *('(?:[^\n]|\\[^\n])+'|(?:\S|\\\S)+))");
    std::smatch what;
    std::map<std::string, std::string> params;
    std::string str = connInfo;

    while (std::regex_search(str, what, re))
    {
        assert(what.size() == 3);
        std::string key = what[1];
        std::string rawValue = what[2];
        std::string value;
        bool quoted =
            !rawValue.empty() && rawValue[0] == '\'' && rawValue.back() == '\'';

        value.reserve(rawValue.size());
        for (size_t i = 0; i < rawValue.size(); i++)
        {
            if (quoted && (i == 0 || i == rawValue.size() - 1))
                continue;
            else if (rawValue[i] == '\\' && i != rawValue.size() - 1)
            {
                value.push_back(rawValue[i + 1]);
                i += 1;
                continue;
            }

            value.push_back(rawValue[i]);
        }
        params[key] = value;
        str = what.suffix().str();
    }
    return params;
}

void DbConnection::sendSubscribe(
    const std::shared_ptr<DbSubscribeContext>& subCtx)
{
    // Note: Can not use placeholders in LISTEN or NOTIFY command!!!
    execSql(
        subCtx->subscribeCommand(),
        0,
        {},
        {},
        {},
        [subCtx, this](const Result& r) {
            // NOTE: we don't necessarily need to store subCtx in DbConnection,
            // but doing so will save the locks when receiving message.
            setSubscribeContext(subCtx);
            LOG_DEBUG << "Subscribe success to " << subCtx->channel();
        },
        [subCtx](const std::exception_ptr& ex) {
            LOG_ERROR << "Failed to subscribe to " << subCtx->channel();
        });
}

void DbConnection::sendUnsubscribe(
    const std::shared_ptr<DbSubscribeContext>& subCtx)
{
    execSql(
        subCtx->unsubscribeCommand(),
        0,
        {},
        {},
        {},
        [subCtx, this](const Result& r) {
            delSubscribeContext(subCtx);
            LOG_DEBUG << "Subscribe success to " << subCtx->channel();
        },
        [subCtx](const std::exception_ptr& ex) {
            // TODO: did not handle exceptions caused by query buffer full
            LOG_ERROR << "Failed to subscribe to " << subCtx->channel();
        });
}
