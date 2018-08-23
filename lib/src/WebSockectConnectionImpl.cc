#include "WebSockectConnectionImpl.h"
#include <trantor/net/TcpConnection.h>

using namespace drogon;
WebSocketConnectionImpl::WebSocketConnectionImpl(const trantor::TcpConnectionPtr &conn):
        _tcpConn(conn),
        _localAddr(conn->localAddr()),
        _peerAddr(conn->peerAddr())
{

}
void WebSocketConnectionImpl::send(const char *msg,uint64_t len)
{
    LOG_TRACE<<"send "<<len<<" bytes";
    auto conn=_tcpConn.lock();
    if(conn)
    {
        //format the frame
        std::string bytesFormatted;
        bytesFormatted.resize(len+10);
        bytesFormatted[0] = char(129);

        int indexStartRawData = -1;
        // set here - it will be set now:

        if (len<=125) {
            bytesFormatted[1] = len;

            indexStartRawData = 2;
        }
        else if(len>= 126 && len <= 65535)
        {
            bytesFormatted[1] = 126;
            bytesFormatted[2] = (( len >> 8 ) & 255);
            bytesFormatted[3] = (( len      ) & 255);
            LOG_TRACE<<"bytes[2]="<<(size_t)bytesFormatted[2];
            LOG_TRACE<<"bytes[3]="<<(size_t)bytesFormatted[3];
            indexStartRawData = 4;
        }
        else
        {
            bytesFormatted[1] = 127;
            bytesFormatted[2] = (( len >> 56 ) & 255);
            bytesFormatted[3] = (( len >> 48 ) & 255);
            bytesFormatted[4] = (( len >> 40 ) & 255);
            bytesFormatted[5] = (( len >> 32 ) & 255);
            bytesFormatted[6] = (( len >> 24 ) & 255);
            bytesFormatted[7] = (( len >> 16 ) & 255);
            bytesFormatted[8] = (( len >>  8 ) & 255);
            bytesFormatted[9] = (( len       ) & 255);

            indexStartRawData = 10;
        }

        bytesFormatted.resize(indexStartRawData);
        LOG_TRACE<<"fheadlen="<<bytesFormatted.length();
        bytesFormatted.append(msg,len);
        LOG_TRACE<<"send formatted frame len="<<len<<" flen="<<bytesFormatted.length();
        conn->send(bytesFormatted);
    }
}
void WebSocketConnectionImpl::send(const std::string &msg)
{
    send(msg.data(),msg.length());
}
const trantor::InetAddress& WebSocketConnectionImpl::localAddr() const
{
    return _localAddr;
}
const trantor::InetAddress& WebSocketConnectionImpl::peerAddr() const
{
    return _peerAddr;
}

bool WebSocketConnectionImpl::connected() const
{
    auto conn=_tcpConn.lock();
    if(conn)
    {
        return conn->connected();
    }
    return false;
}
bool WebSocketConnectionImpl::disconnected() const
{
    auto conn=_tcpConn.lock();
    if(conn)
    {
        return conn->disconnected();
    }
    return true;
}
void WebSocketConnectionImpl::WebSocketConnectionImpl::shutdown()
{
    auto conn=_tcpConn.lock();
    if(conn)
    {
        conn->shutdown();
    }
}
void WebSocketConnectionImpl::WebSocketConnectionImpl::forceClose()
{
    auto conn=_tcpConn.lock();
    if(conn)
    {
        conn->forceClose();
    }
}

void WebSocketConnectionImpl::setContext(const any& context)
{
    _context=context;
}
const any& WebSocketConnectionImpl::WebSocketConnectionImpl::getContext() const
{
    return _context;
}
any* WebSocketConnectionImpl::WebSocketConnectionImpl::getMutableContext()
{
    return &_context;
}