/**
 *
 *  press.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by the MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include "CommandHandler.h"
#include <drogon/DrObject.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <trantor/utils/Date.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <string>
#include <atomic>
#include <memory>
#include <vector>

using namespace drogon;

namespace drogon_ctl
{
struct Statistics
{
    std::atomic_size_t _numOfRequestsSent = ATOMIC_VAR_INIT(0);
    std::atomic_size_t _bytesRecieved = ATOMIC_VAR_INIT(0);
    std::atomic_size_t _numOfGoodResponse = ATOMIC_VAR_INIT(0);
    std::atomic_size_t _numOfBadResponse = ATOMIC_VAR_INIT(0);
    std::atomic_size_t _totalDelay = ATOMIC_VAR_INIT(0);
    trantor::Date _startDate;
    trantor::Date _endDate;
};
class press : public DrObject<press>, public CommandHandler
{
  public:
    virtual void handleCommand(std::vector<std::string> &parameters) override;
    virtual std::string script() override
    {
        return "Do stress testing(Use 'drogon_ctl help press' for more "
               "information)";
    }
    virtual bool isTopCommand() override
    {
        return true;
    }
    virtual std::string detail() override;

  private:
    size_t _numOfThreads = 1;
    size_t _numOfRequests = 1;
    size_t _numOfConnections = 1;
    // bool _keepAlive = false;
    bool _processIndication = true;
    std::string _url;
    std::string _host;
    std::string _path;
    void doTesting();
    void createRequestAndClients();
    void sendRequest(const HttpClientPtr &client);
    void outputResults();
    std::unique_ptr<trantor::EventLoopThreadPool> _loopPool;
    std::vector<HttpClientPtr> _clients;
    Statistics _stat;
};
}  // namespace drogon_ctl
