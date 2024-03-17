/**
 *
 *  DbClientManagerSkipped.cc
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

#include "DbClientManager.h"
#include <drogon/config.h>
#include <drogon/utils/Utilities.h>
#include <algorithm>
#include <stdlib.h>

using namespace drogon::orm;
using namespace drogon;

void DbClientManager::createDbClients(
    const std::vector<trantor::EventLoop *> & /*ioloops*/)
{
    return;
}

void DbClientManager::createDbClient(
    const std::string & /*dbType*/,
    const std::string & /*host*/,
    unsigned short /*port*/,
    const std::string & /*databaseName*/,
    const std::string & /*userName*/,
    const std::string & /*password*/,
    size_t /*connectionNum*/,
    const std::string & /*filename*/,
    const std::string & /*name*/,
    bool /*isFast*/,
    const std::string & /*characterSet*/,
    double /*timeout*/,
    bool /*autoBatch*/,
    const std::unordered_map<std::string, std::string> & /*connectOptions*/)
{
    LOG_FATAL << "No database is supported by drogon, please install the "
                 "database development library first.";
    abort();
}

bool DbClientManager::areAllDbClientsAvailable() const noexcept
{
    LOG_FATAL << "No database is supported by drogon, please install the "
                 "database development library first.";
    abort();
}

DbClientManager::~DbClientManager()
{
}
