/**
 *
 *  WebSocketConnectionImpl.cc
 *  An Tao
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
#include <trantor/net/TcpConnection.h>
#include <trantor/net/inner/TcpConnectionImpl.h>
#include <thread>

using namespace drogon;
WebSocketConnectionImpl::WebSocketConnectionImpl(const trantor::TcpConnectionPtr &conn, bool isServer)
    : _tcpConn(conn),
      _localAddr(conn->localAddr()),
      _peerAddr(conn->peerAddr()),
      _isServer(isServer)
{
}

void WebSocketConnectionImpl::send(const char *msg, uint64_t len, const WebSocketMessageType &type)
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

void WebSocketConnectionImpl::sendWsData(const char *msg, size_t len, unsigned char opcode)
{

    LOG_TRACE << "send " << len << " bytes";

    //Format the frame
    std::string bytesFormatted;
    bytesFormatted.resize(len + 10);
    bytesFormatted[0] = char(0x80 | (opcode & 0x0f));

    int indexStartRawData = -1;

    if (len <= 125)
    {
        bytesFormatted[1] = len;
        indexStartRawData = 2;
    }
    else if (len <= 65535)
    {
        bytesFormatted[1] = 126;
        bytesFormatted[2] = ((len >> 8) & 255);
        bytesFormatted[3] = ((len)&255);
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
        bytesFormatted[9] = ((len)&255);

        indexStartRawData = 10;
    }
    if (!_isServer)
    {
        //Add masking key;
        static std::once_flag once;
        std::call_once(once, []() {
            std::srand(time(nullptr));
        });
        int random = std::rand();

        bytesFormatted[1] = (bytesFormatted[1] | 0x80);
        bytesFormatted.resize(indexStartRawData + 4 + len);
        *((int *)&bytesFormatted[indexStartRawData]) = random;
        for (size_t i = 0; i < len; i++)
        {
            bytesFormatted[indexStartRawData + 4 + i] = (msg[i] ^ bytesFormatted[indexStartRawData + (i % 4)]);
        }
    }
    else
    {
        bytesFormatted.resize(indexStartRawData);
        bytesFormatted.append(msg, len);
    }
    _tcpConn->send(bytesFormatted);
}
void WebSocketConnectionImpl::send(const std::string &msg, const WebSocketMessageType &type)
{
    send(msg.data(), msg.length(), type);
}
const trantor::InetAddress &WebSocketConnectionImpl::localAddr() const
{
    return _localAddr;
}
const trantor::InetAddress &WebSocketConnectionImpl::peerAddr() const
{
    return _peerAddr;
}

bool WebSocketConnectionImpl::connected() const
{
    return _tcpConn->connected();
}
bool WebSocketConnectionImpl::disconnected() const
{
    return _tcpConn->disconnected();
}
void WebSocketConnectionImpl::WebSocketConnectionImpl::shutdown()
{
    _tcpConn->shutdown();
}
void WebSocketConnectionImpl::WebSocketConnectionImpl::forceClose()
{
    _tcpConn->forceClose();
}

void WebSocketConnectionImpl::setContext(const any &context)
{
    _context = context;
}
const any &WebSocketConnectionImpl::WebSocketConnectionImpl::getContext() const
{
    return _context;
}
any *WebSocketConnectionImpl::WebSocketConnectionImpl::getMutableContext()
{
    return &_context;
}

bool WebSocketMessageParser::parse(trantor::MsgBuffer *buffer)
{
    //According to the rfc6455
    _gotAll = false;
    if (buffer->readableBytes() >= 2)
    {
        unsigned char opcode = (*buffer)[0] & 0x0f;
        bool isControlFrame = false;
        switch (opcode)
        {
        case 0:
            //continuation frame
            break;
        case 1:
            _type = WebSocketMessageType::Text;
            break;
        case 2:
            _type = WebSocketMessageType::Binary;
            break;
        case 8:
            _type = WebSocketMessageType::Close;
            isControlFrame = true;
            break;
        case 9:
            _type = WebSocketMessageType::Ping;
            isControlFrame = true;
            break;
        case 10:
            _type = WebSocketMessageType::Pong;
            isControlFrame = true;
            break;
        default:
            LOG_ERROR << "Unknown frame type";
            return false;
            break;
        }

        bool isFin = (((*buffer)[0] & 0x80) == 0x80);
        if (!isFin && isControlFrame)
        {
            //rfc6455-5.5
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
            LOG_TRACE << "plain data";
        size_t indexFirstMask = 2;

        if (length == 126)
        {
            indexFirstMask = 4;
        }
        else if (length == 127)
        {
            indexFirstMask = 10;
        }
        if (indexFirstMask > 2 && buffer->readableBytes() >= indexFirstMask)
        {
            if (isControlFrame)
            {
                //rfc6455-5.5
                LOG_ERROR << "Bad frame: all control frames MUST have a payload length of 125 bytes or less";
                return false;
            }
            if (indexFirstMask == 4)
            {
                length = (unsigned char)(*buffer)[2];
                length = (length << 8) + (unsigned char)(*buffer)[3];
            }
            else if (indexFirstMask == 10)
            {
                length = (unsigned char)(*buffer)[2];
                length = (length << 8) + (unsigned char)(*buffer)[3];
                length = (length << 8) + (unsigned char)(*buffer)[4];
                length = (length << 8) + (unsigned char)(*buffer)[5];
                length = (length << 8) + (unsigned char)(*buffer)[6];
                length = (length << 8) + (unsigned char)(*buffer)[7];
                length = (length << 8) + (unsigned char)(*buffer)[8];
                length = (length << 8) + (unsigned char)(*buffer)[9];
            }
            else
            {
                LOG_ERROR << "Websock parsing failed!";
                return false;
            }
        }
        if (isMasked != 0)
        {
            if (buffer->readableBytes() >= (indexFirstMask + 4 + length))
            {
                auto masks = buffer->peek() + indexFirstMask;
                int indexFirstDataByte = indexFirstMask + 4;
                auto rawData = buffer->peek() + indexFirstDataByte;
                auto oldLen = _message.length();
                _message.resize(oldLen + length);
                for (size_t i = 0; i < length; i++)
                {
                    _message[oldLen + i] = (rawData[i] ^ masks[i % 4]);
                }
                if (isFin)
                    _gotAll = true;
                buffer->retrieve(indexFirstMask + 4 + length);
                return true;
            }
        }
        else
        {
            if (buffer->readableBytes() >= (indexFirstMask + length))
            {
                auto rawData = buffer->peek() + indexFirstMask;
                _message.append(rawData, length);
                if (isFin)
                    _gotAll = true;
                buffer->retrieve(indexFirstMask + length);
                return true;
            }
        }
    }
    return true;
}