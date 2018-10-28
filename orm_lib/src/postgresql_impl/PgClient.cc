
/**
 *
 *  PgClient.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Implementation of the drogon::orm::PgClient class.
 * 
 */

#include "PgClientImpl.h"
#include <drogon/orm/PgClient.h>
#include <drogon/orm/Exception.h>
#include <thread>
#include <unistd.h>

using namespace drogon::orm;

void PgClient::execSql(const std::string &sql,
                       size_t paraNum,
                       const std::vector<const char *> &parameters,
                       const std::vector<int> &length,
                       const std::vector<int> &format,
                       const ResultCallback &rcb,
                       const std::function<void(const std::exception_ptr &)> &exceptCallback)
{
    // std::thread _thread([=]() {
    //     try
    //     {
    //         throw Failure("exception test!!!");
    //     }
    //     catch (...)
    //     {
    //         exceptCallback(std::current_exception());
    //     }
    // });
    // _thread.detach();
    //  rcb(Result());
    _clientPtr->execSql(sql,paraNum,parameters,length,format,rcb,exceptCallback);
}

PgClient::PgClient(const std::string &connInfo, const size_t connNum):
_clientPtr(new PgClientImpl(connInfo,connNum))
{
}