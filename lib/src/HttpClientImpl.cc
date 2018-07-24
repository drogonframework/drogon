#include "HttpClientImpl.h"
#include "HttpRequestImpl.h"
#include "HttpContext.h"
using namespace drogon;
using namespace std::placeholders;
HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const trantor::InetAddress &addr,
                               bool useSSL):
        _loop(loop),
        _server(addr),
        _useSSL(useSSL)
{

}
void HttpClientImpl::sendRequest(const drogon::HttpRequestPtr &req, const drogon::HttpReqCallback &callback)
{
    _loop->runInLoop([=](){
        sendRequestInLoop(req,callback);
    });
}
void HttpClientImpl::sendRequestInLoop(const drogon::HttpRequestPtr &req, const drogon::HttpReqCallback &callback)
{
    _loop->assertInLoopThread();

    if(!_tcpClient)
    {
        LOG_TRACE<<"New TcpClient,"<<_server.toIpPort();
        _tcpClient=std::make_shared<trantor::TcpClient>(_loop,_server,"httpClient");

#ifdef USE_OPENSSL
        if(_useSSL)
        {
            _tcpClient->enableSSL();
        }
#endif
        auto thisPtr=shared_from_this();
        assert(_reqAndCallbacks.empty());
        _reqAndCallbacks.push(std::make_pair(req,callback));
        _tcpClient->setConnectionCallback([=](const trantor::TcpConnectionPtr& connPtr){
            LOG_TRACE<<"connection callback";
            if(connPtr->connected())
            {
                connPtr->setContext(HttpContext());
                //send request;
                LOG_TRACE<<"Connection established!";
                auto req=thisPtr->_reqAndCallbacks.front().first;
                thisPtr->sendReq(connPtr,req);

            }
            else
            {
                while(!(thisPtr->_reqAndCallbacks.empty()))
                {
                    auto reqCallback=_reqAndCallbacks.front().second;
                    _reqAndCallbacks.pop();
                    reqCallback(ReqResult::NetworkFailure,HttpResponseImpl());
                }
                thisPtr->_tcpClient.reset();
            }
        });
        _tcpClient->setMessageCallback(std::bind(&HttpClientImpl::onRecvMessage,shared_from_this(),_1,_2));
        _tcpClient->connect();
    }
    else
    {
        //send request;
        auto connPtr=_tcpClient->connection();
        if(connPtr&&connPtr->connected())
        {
            if(_reqAndCallbacks.empty())
            {
                sendReq(connPtr,req);
            }
        }
        _reqAndCallbacks.push(std::make_pair(req,callback));
    }

}

void HttpClientImpl::sendReq(const trantor::TcpConnectionPtr &connectorPtr,const HttpRequestPtr &req)
{

    trantor::MsgBuffer buffer;
    auto implPtr=std::dynamic_pointer_cast<HttpRequestImpl>(req);
    assert(implPtr);
    implPtr->appendToBuffer(&buffer);
    LOG_TRACE<<"Send request:"<<std::string(buffer.peek(),buffer.readableBytes());
    connectorPtr->send(buffer.peek(),buffer.readableBytes());
}

void HttpClientImpl::onRecvMessage(const trantor::TcpConnectionPtr &connPtr,trantor::MsgBuffer *msg)
{
    HttpContext* context = any_cast<HttpContext>(connPtr->getMutableContext());

    // LOG_INFO << "###:" << string(buf->peek(), buf->readableBytes());
    if (!context->parseResponse(msg)) {
        assert(!_reqAndCallbacks.empty());
        auto cb=_reqAndCallbacks.front().second;
        cb(ReqResult::BadResponse,HttpResponseImpl());
        _reqAndCallbacks.pop();

        _tcpClient.reset();
        return;
    }

    if (context->resGotAll()) {
        auto &resp=context->responseImpl();
        assert(!_reqAndCallbacks.empty());
        auto cb=_reqAndCallbacks.front().second;
        cb(ReqResult::Ok,resp);
        _reqAndCallbacks.pop();

        if(!_reqAndCallbacks.empty())
        {
            auto req=_reqAndCallbacks.front().first;
            sendReq(connPtr,req);
        }
        else
        {
            if(resp.closeConnection())
            {
                _tcpClient.reset();
            }
        }
        context->reset();
    }
}
