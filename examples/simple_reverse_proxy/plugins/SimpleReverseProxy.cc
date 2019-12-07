/**
 *
 *  plugin_SimpleReverseProxy.cc
 *
 */

#include "SimpleReverseProxy.h"

using namespace drogon;
using namespace my_plugin;

void SimpleReverseProxy::initAndStart(const Json::Value &config)
{
    /// Initialize and start the plugin
    if (config.isMember("backends") && config["backends"].isArray())
    {
        for (auto &backend : config["backends"])
        {
            backendAddrs_.emplace_back(backend.asString());
        }
        if (backendAddrs_.empty())
        {
            LOG_ERROR << "You must set at least one backend";
            abort();
        }
    }
    else
    {
        LOG_ERROR << "Error in configuration";
        abort();
    }
    pipeliningDepth_ = config.get("pipelining", 0).asInt();
    if (config.isMember("same_client_to_same_backend"))
    {
        sameClientToSameBackend_ = config["same_client_to_same_backend"]
                                       .get("enabled", false)
                                       .asBool();
        cacheTimeout_ = config["same_client_to_same_backend"]
                            .get("cache_timeout", 0)
                            .asInt();
    }
    if (sameClientToSameBackend_)
    {
        clientMap_ = std::make_unique<drogon::CacheMap<std::string, size_t>>(
            app().getLoop());
    }
    clients_.init(
        [this](std::vector<HttpClientPtr> &clients, size_t ioLoopIndex) {
            clients.resize(backendAddrs_.size());
        });
    clientIndex_.init(
        [this](size_t &index, size_t ioLoopIndex) { index = ioLoopIndex; });
    drogon::app().registerPreRoutingAdvice([this](const HttpRequestPtr &req,
                                                  AdviceCallback &&callback,
                                                  AdviceChainCallback &&pass) {
        preRouting(req, std::move(callback), std::move(pass));
    });
}

void SimpleReverseProxy::shutdown()
{
}

void SimpleReverseProxy::preRouting(const HttpRequestPtr &req,
                                    AdviceCallback &&callback,
                                    AdviceChainCallback &&)
{
    size_t index;
    auto &clientsVector = *clients_;
    if (sameClientToSameBackend_)
    {
        std::lock_guard<std::mutex> lock(mapMutex_);
        auto ip = req->peerAddr().toIp();
        if (!clientMap_->findAndFetch(ip, index))
        {
            index = (++*clientIndex_) % clientsVector.size();
            clientMap_->insert(ip, index, cacheTimeout_);
        }
    }
    else
    {
        index = ++(*clientIndex_) % clientsVector.size();
    }
    auto &clientPtr = clientsVector[index];
    if (!clientPtr)
    {
        auto &addr = backendAddrs_[index];
        clientPtr = HttpClient::newHttpClient(
            addr, trantor::EventLoop::getEventLoopOfCurrentThread());
        clientPtr->setPipeliningDepth(pipeliningDepth_);
    }
    clientPtr->sendRequest(
        req,
        [callback = std::move(callback)](ReqResult result,
                                         const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok)
            {
                callback(resp);
            }
            else
            {
                auto errResp = HttpResponse::newHttpResponse();
                errResp->setStatusCode(k500InternalServerError);
                callback(errResp);
            }
        });
}