/**
 *
 *  @file WebSocketConnectionImpl.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "WebSocketConnectionImpl.h"
#include "HttpAppFrameworkImpl.h"
#include <drogon/utils/Utilities.h>
#include <json/value.h>
#include <json/writer.h>
#include <thread>
#include <limits>

using namespace drogon;

WebSocketConnectionImpl::WebSocketConnectionImpl(
    const trantor::TcpConnectionPtr &conn,
    bool isServer)
    : tcpConnectionPtr_(conn),
      localAddr_(conn->localAddr()),
      peerAddr_(conn->peerAddr()),
      isServer_(isServer),
      usingMask_(false)
{
}

WebSocketConnectionImpl::~WebSocketConnectionImpl()
{
    shutdown();
}

void WebSocketConnectionImpl::send(const char *msg,
                                   uint64_t len,
                                   const WebSocketMessageType type)
{
    unsigned char opcode;
    if (type == WebSocketMessageType::Text)
        opcode = 1;
    else if (type == WebSocketMessageType::Binary)
        opcode = 2;
    else if (type == WebSocketMessageType::Close)
    {
        assert(len <= 125);
        opcode = 8;
    }
    else if (type == WebSocketMessageType::Ping)
    {
        assert(len <= 125);
        opcode = 9;
    }
    else if (type == WebSocketMessageType::Pong)
    {
        assert(len <= 125);
        opcode = 10;
    }
    else
    {
        opcode = 0;
        assert(0);
    }
    sendWsData(msg, len, opcode);
}

void WebSocketConnectionImpl::sendWsData(const char *msg,
                                         uint64_t len,
                                         unsigned char opcode)
{
    LOG_TRACE << "send " << len << " bytes";

    // Format the frame
    std::string bytesFormatted;
    bytesFormatted.resize(len + 10);
    bytesFormatted[0] = char(0x80 | (opcode & 0x0f));

    int indexStartRawData = -1;

    if (len <= 125)
    {
        bytesFormatted[1] = static_cast<char>(len);
        indexStartRawData = 2;
    }
    else if (len <= 65535)
    {
        bytesFormatted[1] = 126;
        bytesFormatted[2] = ((len >> 8) & 255);
        bytesFormatted[3] = ((len) & 255);
        LOG_TRACE << "bytes[2]=" << (size_t)bytesFormatted[2];
        LOG_TRACE << "bytes[3]=" << (size_t)bytesFormatted[3];
        indexStartRawData = 4;
    }
    else
    {
        bytesFormatted[1] = 127;
        bytesFormatted[2] = ((len >> 56) & 255);
        bytesFormatted[3] = ((len >> 48) & 255);
        bytesFormatted[4] = ((len >> 40) & 255);
        bytesFormatted[5] = ((len >> 32) & 255);
        bytesFormatted[6] = ((len >> 24) & 255);
        bytesFormatted[7] = ((len >> 16) & 255);
        bytesFormatted[8] = ((len >> 8) & 255);
        bytesFormatted[9] = ((len) & 255);

        indexStartRawData = 10;
    }
    if (!isServer_)
    {
        int random;
        // Use the cached randomness if no one else is also using it. Otherwise
        // generate one from scratch.
        if (!usingMask_.exchange(true, std::memory_order_acq_rel))
        {
            if (masks_.empty())
            {
                masks_.resize(16);
                bool status =
                    utils::secureRandomBytes(masks_.data(),
                                             masks_.size() * sizeof(uint32_t));
                if (status == false)
                {
                    LOG_ERROR << "Failed to generate random numbers for "
                                 "WebSocket mask";
                    abort();
                }
            }
            random = masks_.back();
            masks_.pop_back();
            usingMask_.store(false, std::memory_order_release);
        }
        else
        {
            bool status = utils::secureRandomBytes(&random, sizeof(random));
            if (status == false)
            {
                LOG_ERROR
                    << "Failed to generate random numbers for WebSocket mask";
                abort();
            }
        }

        bytesFormatted[1] = (bytesFormatted[1] | 0x80);
        bytesFormatted.resize(indexStartRawData + 4 + len);
        memcpy(&bytesFormatted[indexStartRawData], &random, sizeof(random));
        for (size_t i = 0; i < len; ++i)
        {
            bytesFormatted[indexStartRawData + 4 + i] =
                (msg[i] ^ bytesFormatted[indexStartRawData + (i % 4)]);
        }
    }
    else
    {
        bytesFormatted.resize(indexStartRawData);
        bytesFormatted.append(msg, len);
    }
    tcpConnectionPtr_->send(std::move(bytesFormatted));
}

void WebSocketConnectionImpl::send(const std::string_view msg,
                                   const WebSocketMessageType type)
{
    send(msg.data(), msg.length(), type);
}

void WebSocketConnectionImpl::sendJson(const Json::Value &json,
                                       const WebSocketMessageType type)
{
    static std::once_flag once;
    static Json::StreamWriterBuilder builder;
    std::call_once(once, []() {
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
        if (!app().isUnicodeEscapingUsedInJson())
        {
            builder["emitUTF8"] = true;
        }
        auto &precision = app().getFloatPrecisionInJson();
        if (precision.first != 0)
        {
            builder["precision"] = precision.first;
            builder["precisionType"] = precision.second;
        }
    });
    auto msg = writeString(builder, json);
    send(msg.data(), msg.length(), type);
}

const trantor::InetAddress &WebSocketConnectionImpl::localAddr() const
{
    return localAddr_;
}

const trantor::InetAddress &WebSocketConnectionImpl::peerAddr() const
{
    return peerAddr_;
}

bool WebSocketConnectionImpl::connected() const
{
    return tcpConnectionPtr_->connected();
}

bool WebSocketConnectionImpl::disconnected() const
{
    return tcpConnectionPtr_->disconnected();
}

void WebSocketConnectionImpl::WebSocketConnectionImpl::shutdown(
    const CloseCode code,
    const std::string &reason)
{
    tcpConnectionPtr_->getLoop()->invalidateTimer(pingTimerId_);
    if (!tcpConnectionPtr_->connected())
        return;
    std::string message;
    message.resize(reason.length() + 2);
    auto c = htons(static_cast<unsigned short>(code));
    memcpy(&message[0], &c, 2);
    if (!reason.empty())
        memcpy(&message[2], reason.data(), reason.length());
    send(message, WebSocketMessageType::Close);
    tcpConnectionPtr_->shutdown();
}

void WebSocketConnectionImpl::WebSocketConnectionImpl::forceClose()
{
    tcpConnectionPtr_->forceClose();
}

void WebSocketConnectionImpl::setPingMessage(
    const std::string &message,
    const std::chrono::duration<double> &interval)
{
    auto loop = tcpConnectionPtr_->getLoop();
    if (loop->isInLoopThread())
    {
        setPingMessageInLoop(std::string{message}, interval);
    }
    else
    {
        loop->queueInLoop(
            [msg = message, interval, thisPtr = shared_from_this()]() mutable {
                thisPtr->setPingMessageInLoop(std::move(msg), interval);
            });
    }
}

void WebSocketConnectionImpl::disablePing()
{
    auto loop = tcpConnectionPtr_->getLoop();
    if (loop->isInLoopThread())
    {
        disablePingInLoop();
    }
    else
    {
        loop->queueInLoop(
            [thisPtr = shared_from_this()]() { thisPtr->disablePingInLoop(); });
    }
}

bool WebSocketMessageParser::parse(trantor::MsgBuffer *buffer, bool isServer)
{
    // According to the rfc6455
    gotAll_ = false;
    while (buffer->readableBytes() >= 2)
    {
        if (((*buffer)[0] & 0x70) != 0)
        {
            LOG_ERROR << "Bad frame: reserved bits set without an extension";
            return false;
        }
        unsigned char opcode = (*buffer)[0] & 0x0f;
        bool isControlFrame = false;
        WebSocketMessageType frameType = WebSocketMessageType::Unknown;
        switch (opcode)
        {
            case 0:
                LOG_TRACE << "continuation frame";
                break;
            case 1:
                frameType = WebSocketMessageType::Text;
                break;
            case 2:
                frameType = WebSocketMessageType::Binary;
                break;
            case 8:
                frameType = WebSocketMessageType::Close;
                isControlFrame = true;
                break;
            case 9:
                frameType = WebSocketMessageType::Ping;
                isControlFrame = true;
                break;
            case 10:
                frameType = WebSocketMessageType::Pong;
                isControlFrame = true;
                break;
            default:
                LOG_ERROR << "Unknown frame type";
                return false;
                break;
        }
        if (!isControlFrame && ((opcode == 0) != fragmented_))
        {
            LOG_ERROR << "Bad frame: invalid continuation sequence";
            return false;
        }

        bool isFin = (((*buffer)[0] & 0x80) == 0x80);
        if (!isFin && isControlFrame)
        {
            // rfc6455-5.5
            LOG_ERROR << "Bad frame: all control frames MUST NOT be fragmented";
            return false;
        }
        auto secondByte = (*buffer)[1];
        size_t length = secondByte & 127;
        int isMasked = (secondByte & 0x80);
        if (isMasked != 0)
        {
            LOG_TRACE << "data encoded!";
        }
        else
        {
            LOG_TRACE << "plain data";
            if (isServer)
            {
                // rfc6455-5.1: the server MUST close the connection upon
                // receiving an unmasked frame from a client
                LOG_ERROR << "Bad frame: client frames MUST be masked";
                buffer->retrieveAll();
                return false;
            }
        }
        size_t indexFirstMask = 2;

        if (length == 126)
        {
            indexFirstMask = 4;
        }
        else if (length == 127)
        {
            indexFirstMask = 10;
        }
        if (indexFirstMask > 2)
        {
            if (buffer->readableBytes() < indexFirstMask)
            {
                // Not enough data yet, wait for more.
                return true;
            }
            if (isControlFrame)
            {
                // rfc6455-5.5
                LOG_ERROR << "Bad frame: all control frames MUST have a "
                             "payload length "
                             "of 125 bytes or less";
                return false;
            }
            if (indexFirstMask == 4)
            {
                length = (unsigned char)(*buffer)[2];
                length = (length << 8) + (unsigned char)(*buffer)[3];
            }
            else if (indexFirstMask == 10)
            {
                length = 0;
                for (int i = 2; i <= 9; ++i)
                {
                    if (length > ((std::numeric_limits<size_t>::max)() >> 8))
                    {
                        LOG_ERROR
                            << "Payload length too large to handle safely";
                        return false;
                    }
                    length = (length << 8) + (unsigned char)(*buffer)[i];
                }
            }
            else
            {
                LOG_ERROR << "Websock parsing failed!";
                return false;
            }
        }
        if (isServer && !isControlFrame)
        {
            // The message is sent by the client, check the length (including
            // the data already accumulated from previous frames)
            auto maxSize = HttpAppFrameworkImpl::instance()
                               .getClientMaxWebSocketMessageSize();
            if (length > maxSize || message_.length() > maxSize - length)
            {
                LOG_ERROR << "The size of the WebSocket message is too large!";
                buffer->retrieveAll();
                return false;
            }
        }
        if (frameType == WebSocketMessageType::Close && length == 1)
        {
            LOG_ERROR << "Bad frame: Close payload has only one byte";
            return false;
        }
        size_t headerLen = indexFirstMask + (isMasked != 0 ? 4 : 0);
        if (buffer->readableBytes() < headerLen ||
            length > buffer->readableBytes() - headerLen)
        {
            // Not enough data yet, wait for more.
            return true;
        }

        std::string &payload = isControlFrame ? completedMessage_ : message_;
        if (isControlFrame)
        {
            payload.clear();
        }
        auto rawData = buffer->peek() + headerLen;
        if (isMasked != 0)
        {
            auto masks = buffer->peek() + indexFirstMask;
            auto oldLen = payload.length();
            payload.resize(oldLen + length);
            for (size_t i = 0; i < length; ++i)
            {
                payload[oldLen + i] = (rawData[i] ^ masks[i % 4]);
            }
        }
        else
        {
            payload.append(rawData, length);
        }
        buffer->retrieve(headerLen + length);

        if (isControlFrame)
        {
            if (frameType == WebSocketMessageType::Close &&
                payload.size() > 2 &&
                !utils::isValidUTF8(std::string_view(payload).substr(2)))
            {
                LOG_ERROR << "Bad frame: invalid UTF-8 in Close reason";
                return false;
            }
            completedType_ = frameType;
            gotAll_ = true;
            return true;
        }
        if (opcode != 0)
        {
            type_ = frameType;
        }
        fragmented_ = !isFin;
        if (isFin)
        {
            if (type_ == WebSocketMessageType::Text &&
                !utils::isValidUTF8(message_))
            {
                LOG_ERROR << "Bad frame: invalid UTF-8 in text message";
                return false;
            }
            completedMessage_.swap(message_);
            completedType_ = type_;
            gotAll_ = true;
            return true;
        }
    }
    return true;
}

void WebSocketConnectionImpl::onNewMessage(
    const trantor::TcpConnectionPtr &connPtr,
    trantor::MsgBuffer *buffer)
{
    auto self = shared_from_this();
    if (closeReceived_)
    {
        buffer->retrieveAll();
        return;
    }
    while (buffer->readableBytes() > 0)
    {
        auto success = parser_.parse(buffer, isServer_);
        if (success)
        {
            std::string message;
            WebSocketMessageType type;
            if (parser_.gotAll(message, type))
            {
                if (type == WebSocketMessageType::Ping)
                {
                    // ping
                    send(message, WebSocketMessageType::Pong);
                }
                else if (type == WebSocketMessageType::Close)
                {
                    closeReceived_ = true;
                    // rfc6455-5.5.1: reply with the received close frame so
                    // the peer sees the status code it sent instead of an
                    // abnormal closure (1006). Skip the reply when this side
                    // already started the closing handshake, otherwise the
                    // two ends would keep echoing close frames at each other.
                    if (connPtr->connected())
                    {
                        send(message, WebSocketMessageType::Close);
                    }
                    connPtr->shutdown();
                }
                else if (type == WebSocketMessageType::Unknown)
                {
                    return;
                }
                // LOG_TRACE << "new message received: " << message
                //           << "\n(type=" << (int)type << ")";
                messageCallback_(std::move(message), self, type);
                if (type == WebSocketMessageType::Close)
                {
                    buffer->retrieveAll();
                    return;
                }
            }
            else
            {
                return;
            }
        }
        else
        {
            // Websock error!
            connPtr->shutdown();
            return;
        }
    }
    return;
}

void WebSocketConnectionImpl::disablePingInLoop()
{
    if (pingTimerId_ != trantor::InvalidTimerId)
    {
        tcpConnectionPtr_->getLoop()->invalidateTimer(pingTimerId_);
    }
}

void WebSocketConnectionImpl::setPingMessageInLoop(
    std::string &&message,
    const std::chrono::duration<double> &interval)
{
    std::weak_ptr<WebSocketConnectionImpl> weakPtr = shared_from_this();
    disablePingInLoop();
    pingTimerId_ = tcpConnectionPtr_->getLoop()->runEvery(
        interval.count(), [weakPtr, message = std::move(message)]() {
            auto thisPtr = weakPtr.lock();
            if (thisPtr)
            {
                thisPtr->send(message, WebSocketMessageType::Ping);
            }
        });
}
