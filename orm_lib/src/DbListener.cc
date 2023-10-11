/**
 *
 *  @file DbListener.cc
 *  @author Nitromelon
 *
 *  Copyright 2022, An Tao.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <drogon/config.h>
#include <drogon/orm/DbListener.h>
#include <trantor/utils/Logger.h>
#include <mutex>

#if USE_POSTGRESQL
#include "postgresql_impl/PgListener.h"
#endif

using namespace drogon;
using namespace drogon::orm;

DbListener::~DbListener() = default;

std::shared_ptr<DbListener> DbListener::newPgListener(
    const std::string &connInfo,
    trantor::EventLoop *loop)
{
#if USE_POSTGRESQL
    std::shared_ptr<PgListener> pgListener =
        std::make_shared<PgListener>(connInfo, loop);
    pgListener->init();
    return pgListener;
#else
    LOG_ERROR << "Postgresql is not supported by current drogon build";
    return nullptr;
#endif
}
