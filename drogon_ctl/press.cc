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
#include <stdlib.h>
#include <unistd.h>

using namespace drogon_ctl;
std::string press::detail()
{
    return "Use press command to do stress testing\n"
           "Usage:drogon_ctl press <options> <url>\n"
           "  -n num    number of requests(default : 1)\n"
           "  -t num    number of threads(default : 1)\n"
           "  -c num    concurrent connections(default : 1)\n"
           //  "  -k        keep alive(default: no)\n"
           "  -q        no progress indication(default: no)\n\n"
           "example: drogon_ctl press -n 10000 -c 100 -t 4 -q "
           "http://localhost:8080/index.html\n";
}

void outputErrorAndExit(const string_view &err)
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
                iter++;
                if (iter == parameters.end())
                {
                    outputErrorAndExit("No number of requests!");
                }
                auto &num = *iter;
                try
                {
                    _numOfRequests = std::stoll(num);
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
                    _numOfRequests = std::stoll(num);
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
                iter++;
                if (iter == parameters.end())
                {
                    outputErrorAndExit("No number of threads!");
                }
                auto &num = *iter;
                try
                {
                    _numOfThreads = std::stoll(num);
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
                    _numOfThreads = std::stoll(num);
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
                iter++;
                if (iter == parameters.end())
                {
                    outputErrorAndExit("No number of connections!");
                }
                auto &num = *iter;
                try
                {
                    _numOfConnections = std::stoll(num);
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
                    _numOfConnections = std::stoll(num);
                }
                catch (...)
                {
                    outputErrorAndExit("Invalid number of connections!");
                }
                continue;
            }
        }
        // else if (param == "-k")
        // {
        //     _keepAlive = true;
        //     continue;
        // }
        else if (param == "-q")
        {
            _processIndication = false;
        }
        else if (param[0] != '-')
        {
            _url = param;
        }
    }
    // std::cout << "n=" << _numOfRequests << std::endl;
    // std::cout << "t=" << _numOfThreads << std::endl;
    // std::cout << "c=" << _numOfConnections << std::endl;
    // std::cout << "q=" << _processIndication << std::endl;
    // std::cout << "url=" << _url << std::endl;
    if (_url.empty() || _url.find("http") != 0 ||
        _url.find("://") == std::string::npos)
    {
        outputErrorAndExit("Invalid URL");
    }
    else
    {
        auto pos = _url.find("://");
        auto posOfPath = _url.find("/", pos + 3);
        if (posOfPath == std::string::npos)
        {
            _host = _url;
            _path = "/";
        }
        else
        {
            _host = _url.substr(0, posOfPath);
            _path = _url.substr(posOfPath);
        }
    }
    // std::cout << "host=" << _host << std::endl;
    // std::cout << "path=" << _path << std::endl;
    doTesting();
}

void press::doTesting()
{
    createRequestAndClients();
    if (_clients.empty())
    {
        outputErrorAndExit("No connection!");
    }
    _stat._startDate = trantor::Date::now();
    for (auto &client : _clients)
    {
        sendRequest(client);
    }
    _loopPool->wait();
}

void press::createRequestAndClients()
{
    _loopPool = std::make_unique<trantor::EventLoopThreadPool>(_numOfThreads);
    _loopPool->start();
    for (size_t i = 0; i < _numOfConnections; i++)
    {
        auto client =
            HttpClient::newHttpClient(_host, _loopPool->getNextLoop());
        client->enableCookies();
        _clients.push_back(client);
    }
}

void press::sendRequest(const HttpClientPtr &client)
{
    auto numOfRequest = _stat._numOfRequestsSent++;
    if (numOfRequest >= _numOfRequests)
    {
        return;
    }
    auto request = HttpRequest::newHttpRequest();
    request->setPath(_path);
    request->setMethod(Get);
    // std::cout << "send!" << std::endl;
    client->sendRequest(
        request,
        [this, client, request](ReqResult r, const HttpResponsePtr &resp) {
            size_t goodNum, badNum;
            if (r == ReqResult::Ok)
            {
                // std::cout << "OK" << std::endl;
                goodNum = ++_stat._numOfGoodResponse;
                badNum = _stat._numOfBadResponse;
                _stat._bytesRecieved += resp->body().length();
                auto delay = trantor::Date::now().microSecondsSinceEpoch() -
                             request->creationDate().microSecondsSinceEpoch();
                _stat._totalDelay += delay;
            }
            else
            {
                goodNum = _stat._numOfGoodResponse;
                badNum = ++_stat._numOfBadResponse;
                if (badNum > _numOfRequests / 10)
                {
                    outputErrorAndExit("Too many errors");
                }
            }
            if (goodNum + badNum >= _numOfRequests)
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

            if (_processIndication)
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
    static std::mutex mtx;
    size_t totalSent = 0;
    size_t totalRecv = 0;
    for (auto &client : _clients)
    {
        totalSent += client->bytesSent();
        totalRecv += client->bytesReceived();
    }
    auto now = trantor::Date::now();
    auto microSecs = now.microSecondsSinceEpoch() -
                     _stat._startDate.microSecondsSinceEpoch();
    double seconds = (double)microSecs / 1000000.0;
    size_t rps = _stat._numOfGoodResponse / seconds;
    std::cout << std::endl;
    std::cout << "TOTALS:   " << _numOfConnections << " connect, "
              << _numOfRequests << " requests, " << _stat._numOfGoodResponse
              << " success, " << _stat._numOfBadResponse << " fail"
              << std::endl;

    std::cout << "TRAFFIC:  " << _stat._bytesRecieved / _stat._numOfGoodResponse
              << " avg bytes, "
              << (totalRecv - _stat._bytesRecieved) / _stat._numOfGoodResponse
              << " avg overhead, " << _stat._bytesRecieved << " bytes, "
              << totalRecv - _stat._bytesRecieved << " overhead" << std::endl;

    std::cout << std::setiosflags(std::ios::fixed) << std::setprecision(3)
              << "TIMING:   " << seconds << " seconds, " << rps << " rps, "
              << (double)(_stat._totalDelay) / _stat._numOfGoodResponse / 1000
              << " ms avg req time" << std::endl;

    std::cout << "SPEED:    download " << totalRecv / seconds / 1000
              << " kBps, upload " << totalSent / seconds / 1000 << " kBps"
              << std::endl
              << std::endl;
    exit(0);
}
