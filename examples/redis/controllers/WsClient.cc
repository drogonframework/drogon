#include "WsClient.h"

#include <memory>
#include <unordered_set>

struct ClientContext
{
    std::unordered_set<std::string> channels_;
    std::shared_ptr<nosql::RedisSubscriber> subscriber_;
};

void WsClient::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                                std::string&& message,
                                const WebSocketMessageType& type)
{
    if (type == WebSocketMessageType::Ping ||
        type == WebSocketMessageType::Pong ||
        type == WebSocketMessageType::Close)
    {
        return;
    }

    if (type != WebSocketMessageType::Text)
    {
        LOG_ERROR << "Unsupported message type " << (int)type;
        return;
    }
    LOG_DEBUG << "WsClient new message from "
              << wsConnPtr->peerAddr().toIpPort();

    auto context = wsConnPtr->getContext<ClientContext>();
    if (!context)
    {
        auto pos = message.find(' ');
        if (pos == std::string::npos)
        {
            wsConnPtr->send("Invalid publish message.");
            return;
        }

        std::string channel = message.substr(0, pos);
        std::string msg = message.substr(pos + 1);
        LOG_INFO << "PUBLISH " << channel << " " << msg;

        // Publisher
        drogon::app().getRedisClient()->execCommandAsync(
            [wsConnPtr](const nosql::RedisResult& result) {
                std::string nSubs = std::to_string(result.asInteger());
                LOG_INFO << "PUBLISH success to " << nSubs << " subscribers.";
                wsConnPtr->send("PUBLISH success to " + nSubs +
                                " subscribers.");
            },
            [wsConnPtr](const nosql::RedisException& ex) {
                LOG_INFO << "PUBLISH failed, " << ex.what();
                wsConnPtr->send(std::string("PUBLISH failed: ") + ex.what());
            },
            "PUBLISH %s %s",
            channel.c_str(),
            msg.c_str());
        return;
    }

    std::string channel = std::move(message);
    if (channel.empty())
    {
        wsConnPtr->send("Channel not provided");
        return;
    }

    bool subscribe = true;
    if (channel.compare(0, 6, "unsub ") == 0)
    {
        channel = channel.substr(6);
        subscribe = false;
    }

    if (subscribe)
    {
        if (context->channels_.find(channel) != context->channels_.end())
        {
            wsConnPtr->send("Already subscribed to channel " + channel);
            return;
        }

        context->subscriber_->subscribe(
            channel,
            [channel, wsConnPtr](const std::string& subChannel,
                                 const std::string& subMessage) {
                assert(subChannel == channel);
                LOG_INFO << "Receive channel message " << subMessage;
                std::string resp = "{\"channel\":\"" + subChannel +
                                   "\",\"message\":\"" + subMessage + "\"}";
                wsConnPtr->send(resp);
            });

        context->channels_.insert(channel);
        wsConnPtr->send("Subscribe to channel: " + channel);
    }
    else
    {
        if (context->channels_.find(channel) == context->channels_.end())
        {
            wsConnPtr->send("Channel not subscribed.");
            return;
        }
        context->channels_.erase(channel);
        context->subscriber_->unsubscribe(channel);
        wsConnPtr->send("Unsubscribe from channel: " + channel);
    }
}

void WsClient::handleNewConnection(const HttpRequestPtr& req,
                                   const WebSocketConnectionPtr& wsConnPtr)
{
    if (req->getPath() == "/sub")
    {
        LOG_DEBUG << "WsClient new subscriber connection from "
                  << wsConnPtr->peerAddr().toIpPort();
        std::shared_ptr<ClientContext> context =
            std::make_shared<ClientContext>();
        context->subscriber_ = drogon::app().getRedisClient()->newSubscriber();
        wsConnPtr->setContext(context);
    }
    else
    {
        LOG_DEBUG << "WsClient new publisher connection from "
                  << wsConnPtr->peerAddr().toIpPort();
    }
}

void WsClient::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_DEBUG << "WsClient close connection from "
              << wsConnPtr->peerAddr().toIpPort();
    // Channels will be auto unsubscribed when subscriber destructed.
    // auto context = wsConnPtr->getContext<ClientContext>();
    // for (auto& channel : context->channels_)
    // {
    //     context->subscriber_->unsubscribe(channel);
    // }
    wsConnPtr->clearContext();
}
