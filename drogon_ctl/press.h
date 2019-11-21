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
    std::atomic_size_t numOfRequestsSent_{0};
    std::atomic_size_t bytesRecieved_{0};
    std::atomic_size_t numOfGoodResponse_{0};
    std::atomic_size_t numOfBadResponse_{0};
    std::atomic_size_t totalDelay_{0};
    trantor::Date startDate_;
    trantor::Date endDate_;
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
    size_t numOfThreads_{1};
    size_t numOfRequests_{1};
    size_t numOfConnections_{1};
    // bool keepAlive_ = false;
    bool processIndication_{true};
    std::string url_;
    std::string host_;
    std::string path_;
    void doTesting();
    void createRequestAndClients();
    void sendRequest(const HttpClientPtr &client);
    void outputResults();
    std::unique_ptr<trantor::EventLoopThreadPool> loopPool_;
    std::vector<HttpClientPtr> clients_;
    Statistics statistics_;
};
}  // namespace drogon_ctl
