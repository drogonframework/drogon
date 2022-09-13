#include "Chat.h"

#include <memory>
#include <unordered_set>

static void redisLogin(std::function<void(int)>&& callback,
                       const std::string& loginKey,
                       unsigned int timeout);
static void redisLogout(std::function<void(int)>&& callback,
                        const std::string& loginKey);

struct ClientContext
{
    std::string name_;
    std::string loginKey_;
    std::string room_;
    std::shared_ptr<nosql::RedisSubscriber> subscriber_;
};

static bool checkRoomNumber(const std::string& room)
{
    if (room.empty() || room.size() > 2 || (room.size() == 2 && room[0] == '0'))
    {
        return false;
    }
    for (char c : room)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
    }
    return true;
}

void Chat::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                            std::string&& message,
                            const WebSocketMessageType& type)
{
    if (type == WebSocketMessageType::Close ||
        type == WebSocketMessageType::Ping)
    {
        return;
    }
    if (type != WebSocketMessageType::Text &&
        type != WebSocketMessageType::Pong)
    {
        LOG_ERROR << "Unsupported message type " << (int)type;
        return;
    }
    LOG_DEBUG << "WsClient new message from "
              << wsConnPtr->peerAddr().toIpPort();

    auto context = wsConnPtr->getContext<ClientContext>();
    if (!context || context->name_.empty())
    {
        wsConnPtr->send("ERROR: You are not logged in.");
        wsConnPtr->forceClose();
        return;
    }

    auto redisClient = drogon::app().getRedisClient();
    if (type == WebSocketMessageType::Pong)
    {
        redisClient->execCommandAsync(
            [wsConnPtr](const nosql::RedisResult&) {
                // Do nothing
            },
            [wsConnPtr](const nosql::RedisException& ex) {
                LOG_ERROR << "Update user status failed: " << ex.what();
                wsConnPtr->send("ERROR: Service unavailable.");
                wsConnPtr->forceClose();
            },
            "SET %s 1 EX 120",
            context->loginKey_.c_str());
        return;
    }

    int operation = 0;
    std::string room;
    if (message.compare(0, 6, "ENTER ") == 0)
    {
        room = message.substr(6);
        if (!checkRoomNumber(room))
        {
            wsConnPtr->send("ERROR: Invalid room number, should be [0-99].");
            return;
        }
        operation = 1;
    }
    else if (message == "QUIT")
    {
        operation = 2;
    }

    switch (operation)
    {
        case 0:  // Message
        {
            if (context->room_.empty())
            {
                wsConnPtr->send(
                    "ERROR: Not in room. Send 'ENTER roomNo' to enter a "
                    "room first.");
                return;
            }
            // Publish message
            std::string msg =
                "[" + context->room_ + "][" + context->name_ + "] " + message;

            // NOTICE: Dangerous to concat username into redis command!!!
            // Do not use in production.
            redisClient->execCommandAsync(
                [](const nosql::RedisResult&) {},
                [wsConnPtr](const nosql::RedisException& ex) {
                    wsConnPtr->send(std::string("ERROR: ") + ex.what());
                },
                "publish %s %s",
                context->room_.c_str(),
                msg.c_str());
            break;
        }
        case 1:  // Enter room
        {
            if (context->room_ == room)
            {
                wsConnPtr->send("ERROR: Already in room " + context->room_);
                return;
            }
            if (!context->room_.empty())
            {
                context->subscriber_->unsubscribe(context->room_);
            }
            wsConnPtr->send("INFO: Enter room " + room);
            context->subscriber_->subscribe(
                room, [wsConnPtr](const std::string&, const std::string& msg) {
                    wsConnPtr->send(msg);
                });
            context->room_ = room;
            break;
        }
        case 2:  // Quit room
        {
            if (!context->room_.empty())
            {
                context->subscriber_->unsubscribe(context->room_);
                wsConnPtr->send("INFO: Quit room " + context->room_);
                context->room_.clear();
            }
            else
            {
                wsConnPtr->send(
                    "ERROR: Not in room. Send 'ENTER roomNo' to enter a "
                    "room first.");
            }
            break;
        }
        default:
            break;
    }
}

void Chat::handleNewConnection(const HttpRequestPtr& req,
                               const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_DEBUG << "WsClient new connection from "
              << wsConnPtr->peerAddr().toIpPort();
    const std::string name = req->getParameter("name");
    if (name.empty())
    {
        wsConnPtr->send("Please give your name in parameters.");
        wsConnPtr->forceClose();
    }

    std::string loginKey = "redis_chat:user:" + drogon::utils::getMd5(name);
    redisLogin(
        [wsConnPtr, name, loginKey](int status) {
            if (status < 0)
            {
                wsConnPtr->send("ERROR: Service unavailable.");
                wsConnPtr->shutdown();
                return;
            }
            if (status == 0)
            {
                wsConnPtr->send("ERROR: User [" + name +
                                "] already logged in.");
                wsConnPtr->shutdown();
                return;
            }
            std::shared_ptr<ClientContext> context =
                std::make_shared<ClientContext>();
            context->subscriber_ =
                drogon::app().getRedisClient()->newSubscriber();
            context->name_ = name;
            context->loginKey_ = loginKey;
            wsConnPtr->setContext(context);
            wsConnPtr->send("Hello, " + name + "!");
        },
        loginKey,
        120);
}

void Chat::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)
{
    LOG_DEBUG << "WsClient close connection from "
              << wsConnPtr->peerAddr().toIpPort();
    auto context = wsConnPtr->getContext<ClientContext>();
    // Channels will be auto unsubscribed when subscriber destructed.
    // for (auto& channel : context->channels_)
    // {
    //     context->subscriber_->unsubscribe(channel);
    // }
    if (context)
    {
        redisLogout([](int) {}, context->loginKey_);
    }
}

static void redisLogin(std::function<void(int)>&& callback,
                       const std::string& loginKey,
                       unsigned int timeout)
{
    static const char script[] = R"(
local exists = redis.call('GET', KEYS[1]);
if exists then return 0 end;
redis.call('SET', KEYS[1], 1, 'EX', ARGV[1]);
return 1;
)";

    drogon::app().getRedisClient()->execCommandAsync(
        [callback](const nosql::RedisResult& result) {
            callback((int)result.asInteger());
        },
        [callback](const nosql::RedisException& ex) {
            LOG_ERROR << "Login error: " << ex.what();
            callback(-1);
        },
        "EVAL %s 1 %s %u",
        script,
        loginKey.c_str(),
        timeout);
}

static void redisLogout(std::function<void(int)>&& callback,
                        const std::string& loginKey)
{
    drogon::app().getRedisClient()->execCommandAsync(
        [callback](const nosql::RedisResult& result) {
            callback((int)result.asInteger());
        },
        [callback](const nosql::RedisException& ex) {
            LOG_ERROR << "Logout error: " << ex.what();
            callback(-1);
        },
        "DEL %s",
        loginKey.c_str());
}
