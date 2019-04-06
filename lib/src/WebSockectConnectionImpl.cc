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

#include "WebSockectConnectionImpl.h"
#include <trantor/net/TcpConnection.h>
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
        opcode = 8;
    else if (type == WebSocketMessageType::Ping)
        opcode = 9;
    else if (type == WebSocketMessageType::Pong)
        opcode = 10;
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
    auto conn = _tcpConn.lock();
    if (conn)
    {
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

        conn->send(bytesFormatted);
    }
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
    auto conn = _tcpConn.lock();
    if (conn)
    {
        return conn->connected();
    }
    return false;
}
bool WebSocketConnectionImpl::disconnected() const
{
    auto conn = _tcpConn.lock();
    if (conn)
    {
        return conn->disconnected();
    }
    return true;
}
void WebSocketConnectionImpl::WebSocketConnectionImpl::shutdown()
{
    auto conn = _tcpConn.lock();
    if (conn)
    {
        conn->shutdown();
    }
}
void WebSocketConnectionImpl::WebSocketConnectionImpl::forceClose()
{
    auto conn = _tcpConn.lock();
    if (conn)
    {
        conn->forceClose();
    }
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

