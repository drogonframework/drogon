/**
 *
 *  press.cc
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

#include "press.h"
#include "cmd.h"
#include <drogon/DrClassMap.h>
#include <iostream>
#include <memory>
#include <iomanip>
#include <cstdlib>
#include <json/json.h>
#include <fstream>
#include <string>
#include <unordered_map>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace drogon_ctl;

std::string press::detail()
{
    return "Use press command to do stress testing\n"
           "Usage:drogon_ctl press <options> <url>\n"
           "  -n num    number of requests(default : 1)\n"
           "  -t num    number of threads(default : 1)\n"
           "  -c num    concurrent connections(default : 1)\n"
           "  -k        disable SSL certificate validation(default: enable)\n"
           "  -f        customize http request json file(default: disenable)\n"
           "  -q        no progress indication(default: show)\n\n"
           "example: drogon_ctl press -n 10000 -c 100 -t 4 -q "
           "http://localhost:8080/index.html -f ./http_request.json\n";
}

void outputErrorAndExit(const std::string_view &err)
{
    std::cout << err << std::endl;
    exit(1);
}

void press::handleCommand(std::vector<std::string> &parameters)
{
    for (auto iter = parameters.begin(); iter != parameters.end(); iter++)
    {
        auto &param = *iter;
        if (param.find("-n") == 0)
        {
            if (param == "-n")
            {
                ++iter;
                if (iter == parameters.end())
                {
                    outputErrorAndExit("No number of requests!");
                }
                auto &num = *iter;
                try
                {
                    numOfRequests_ = std::stoll(num);
                }
                catch (...)
                {
                    outputErrorAndExit("Invalid number of requests!");
                }
                continue;
            }
            else
            {
                auto num = param.substr(2);
                try
                {
                    numOfRequests_ = std::stoll(num);
                }
                catch (...)
                {
                    outputErrorAndExit("Invalid number of requests!");
                }
                continue;
            }
        }
        else if (param.find("-t") == 0)
        {
            if (param == "-t")
            {
                ++iter;
                if (iter == parameters.end())
                {
                    outputErrorAndExit("No number of threads!");
                }
                auto &num = *iter;
                try
                {
                    numOfThreads_ = std::stoll(num);
                }
                catch (...)
                {
                    outputErrorAndExit("Invalid number of threads!");
                }
                continue;
            }
            else
            {
                auto num = param.substr(2);
                try
                {
                    numOfThreads_ = std::stoll(num);
                }
                catch (...)
                {
                    outputErrorAndExit("Invalid number of threads!");
                }
                continue;
            }
        }
        else if (param.find("-c") == 0)
        {
            if (param == "-c")
            {
                ++iter;
                if (iter == parameters.end())
                {
                    outputErrorAndExit("No number of connections!");
                }
                auto &num = *iter;
                try
                {
                    numOfConnections_ = std::stoll(num);
                }
                catch (...)
                {
                    outputErrorAndExit("Invalid number of connections!");
                }
                continue;
            }
            else
            {
                auto num = param.substr(2);
                try
                {
                    numOfConnections_ = std::stoll(num);
                }
                catch (...)
                {
                    outputErrorAndExit("Invalid number of connections!");
                }
                continue;
            }
        }
        else if (param.find("-f") == 0)
        {
            if (param == "-f")
            {
                ++iter;
                if (iter == parameters.end())
                {
                    outputErrorAndExit("No http request json file!");
                }
                httpRequestJsonFile_ = *iter;
                continue;
            }
            else
            {
                httpRequestJsonFile_ = param.substr(2);
                continue;
            }
        }
        else if (param == "-k")
        {
            certValidation_ = false;
            continue;
        }
        else if (param == "-q")
        {
            processIndication_ = false;
        }
        else if (param[0] != '-')
        {
            url_ = param;
        }
    }
    // std::cout << "n=" << numOfRequests_ << std::endl;
    // std::cout << "t=" << numOfThreads_ << std::endl;
    // std::cout << "c=" << numOfConnections_ << std::endl;
    // std::cout << "q=" << processIndication_ << std::endl;
    // std::cout << "url=" << url_ << std::endl;
    if (url_.empty() || url_.compare(0, 4, "http") != 0 ||
        (url_.compare(4, 3, "://") != 0 && url_.compare(4, 4, "s://") != 0))
    {
        outputErrorAndExit("Invalid URL");
    }
    else
    {
        auto pos = url_.find("://");
        auto posOfPath = url_.find('/', pos + 3);
        if (posOfPath == std::string::npos)
        {
            host_ = url_;
            path_ = "/";
        }
        else
        {
            host_ = url_.substr(0, posOfPath);
            path_ = url_.substr(posOfPath);
        }
    }

    /*
    http_request.json
    {
        "method": "POST",
        "header": {
            "token": "e2e9d0fe-dd14-4eaf-8ac1-0997730a805d"
        },
        "body": {
            "passwd": "123456",
            "account": "10001"
        }
    }
    */
    if (!httpRequestJsonFile_.empty())
    {
        Json::Value httpRequestJson;
        std::ifstream httpRequestFile(httpRequestJsonFile_,
                                      std::ifstream::binary);
        if (!httpRequestFile.is_open())
        {
            outputErrorAndExit(std::string{"No "} + httpRequestJsonFile_);
        }
        httpRequestFile >> httpRequestJson;

        if (!httpRequestJson.isMember("method"))
        {
            outputErrorAndExit("No contain method");
        }

        auto methodStr = httpRequestJson["method"].asString();
        std::transform(methodStr.begin(),
                       methodStr.end(),
                       methodStr.begin(),
                       ::toupper);

        auto toHttpMethod = [&]() -> drogon::HttpMethod {
            if (methodStr == "GET")
            {
                return drogon::HttpMethod::Get;
            }
            else if (methodStr == "POST")
            {
                return drogon::HttpMethod::Post;
            }
            else if (methodStr == "HEAD")
            {
                return drogon::HttpMethod::Head;
            }
            else if (methodStr == "PUT")
            {
                return drogon::HttpMethod::Put;
            }
            else if (methodStr == "DELETE")
            {
                return drogon::HttpMethod::Delete;
            }
            else if (methodStr == "OPTIONS")
            {
                return drogon::HttpMethod::Options;
            }
            else if (methodStr == "PATCH")
            {
                return drogon::HttpMethod::Patch;
            }
            else
            {
                outputErrorAndExit("invalid method");
            }
            return drogon::HttpMethod::Get;
        };

        std::unordered_map<std::string, std::string> header;
        if (httpRequestJson.isMember("header"))
        {
            auto &jsonValue = httpRequestJson["header"];
            for (const auto &key : jsonValue.getMemberNames())
            {
                if (jsonValue[key].isString())
                {
                    header[key] = jsonValue[key].asString();
                }
                else
                {
                    header[key] = jsonValue[key].toStyledString();
                }
            }
        }

        std::string body;
        if (httpRequestJson.isMember("body"))
        {
            Json::FastWriter fastWriter;
            body = fastWriter.write(httpRequestJson["body"]);
        }

        createHttpRequestFunc_ = [this,
                                  method = toHttpMethod(),
                                  body = std::move(body),
                                  header =
                                      std::move(header)]() -> HttpRequestPtr {
            auto request = HttpRequest::newHttpRequest();
            request->setPath(path_);
            request->setMethod(method);
            for (const auto &[field, val] : header)
                request->addHeader(field, val);
            if (!body.empty())
                request->setBody(body);
            return request;
        };
    }

    // std::cout << "host=" << host_ << std::endl;
    // std::cout << "path=" << path_ << std::endl;
    doTesting();
}

void press::doTesting()
{
    createRequestAndClients();
    if (clients_.empty())
    {
        outputErrorAndExit("No connection!");
    }
    statistics_.startDate_ = trantor::Date::now();
    for (auto &client : clients_)
    {
        sendRequest(client);
    }
    loopPool_->wait();
}

void press::createRequestAndClients()
{
    loopPool_ = std::make_unique<trantor::EventLoopThreadPool>(numOfThreads_);
    loopPool_->start();
    for (size_t i = 0; i < numOfConnections_; ++i)
    {
        auto client = HttpClient::newHttpClient(host_,
                                                loopPool_->getNextLoop(),
                                                false,
                                                certValidation_);
        client->enableCookies();
        clients_.push_back(client);
    }
}

void press::sendRequest(const HttpClientPtr &client)
{
    auto numOfRequest = statistics_.numOfRequestsSent_++;
    if (numOfRequest >= numOfRequests_)
    {
        return;
    }

    HttpRequestPtr request;
    if (createHttpRequestFunc_)
    {
        request = createHttpRequestFunc_();
    }
    else
    {
        request = HttpRequest::newHttpRequest();
        request->setPath(path_);
        request->setMethod(Get);
    }

    // std::cout << "send!" << std::endl;
    client->sendRequest(
        request,
        [this, client, request](ReqResult r, const HttpResponsePtr &resp) {
            size_t goodNum, badNum;
            if (r == ReqResult::Ok)
            {
                // std::cout << "OK" << std::endl;
                goodNum = ++statistics_.numOfGoodResponse_;
                badNum = statistics_.numOfBadResponse_;
                statistics_.bytesRecieved_ += resp->body().length();
                auto delay = trantor::Date::now().microSecondsSinceEpoch() -
                             request->creationDate().microSecondsSinceEpoch();
                statistics_.totalDelay_ += delay;
            }
            else
            {
                goodNum = statistics_.numOfGoodResponse_;
                badNum = ++statistics_.numOfBadResponse_;
                if (badNum > numOfRequests_ / 10)
                {
                    outputErrorAndExit("Too many errors");
                }
            }
            if (goodNum + badNum >= numOfRequests_)
            {
                outputResults();
            }
            if (r == ReqResult::Ok)
                sendRequest(client);
            else
            {
                client->getLoop()->runAfter(1, [this, client]() {
                    sendRequest(client);
                });
            }

            if (processIndication_)
            {
                auto rec = goodNum + badNum;
                if (rec % 100000 == 0)
                {
                    std::cout << rec << " responses are received" << std::endl
                              << std::endl;
                }
            }
        });
}

void press::outputResults()
{
    size_t totalSent = 0;
    size_t totalRecv = 0;
    for (auto &client : clients_)
    {
        totalSent += client->bytesSent();
        totalRecv += client->bytesReceived();
    }
    auto now = trantor::Date::now();
    auto microSecs = now.microSecondsSinceEpoch() -
                     statistics_.startDate_.microSecondsSinceEpoch();
    double seconds = (double)microSecs / 1000000.0;
    auto rps = static_cast<size_t>(statistics_.numOfGoodResponse_ / seconds);
    std::cout << std::endl;
    std::cout << "TOTALS:   " << numOfConnections_ << " connect, "
              << numOfRequests_ << " requests, "
              << statistics_.numOfGoodResponse_ << " success, "
              << statistics_.numOfBadResponse_ << " fail" << std::endl;

    std::cout << "TRAFFIC:  "
              << statistics_.bytesRecieved_ / statistics_.numOfGoodResponse_
              << " avg bytes, "
              << (totalRecv - statistics_.bytesRecieved_) /
                     statistics_.numOfGoodResponse_
              << " avg overhead, " << statistics_.bytesRecieved_ << " bytes, "
              << totalRecv - statistics_.bytesRecieved_ << " overhead"
              << std::endl;

    std::cout << std::setiosflags(std::ios::fixed) << std::setprecision(3)
              << "TIMING:   " << seconds << " seconds, " << rps << " rps, "
              << (double)(statistics_.totalDelay_) /
                     statistics_.numOfGoodResponse_ / 1000
              << " ms avg req time" << std::endl;

    std::cout << "SPEED:    download " << totalRecv / seconds / 1000
              << " kBps, upload " << totalSent / seconds / 1000 << " kBps"
              << std::endl
              << std::endl;
    exit(0);
}
