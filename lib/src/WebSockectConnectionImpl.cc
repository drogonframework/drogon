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
    auto conn=_tcpConn.lock();
    if(conn)
    {
        conn->send(msg,len);
    }
}
void WebSocketConnectionImpl::send(const std::string &msg)
{
    auto conn=_tcpConn.lock();
    if(conn)
    {
        conn->send(msg);
    }
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