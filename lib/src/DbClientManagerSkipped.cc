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
#include <algorithm>
#include <stdlib.h>

using namespace drogon::orm;
using namespace drogon;

void DbClientManager::createDbClients(
    const std::vector<trantor::EventLoop *> & /*ioLoops*/)
{
    return;
}

void DbClientManager::addDbClient(const DbConfig &)
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
