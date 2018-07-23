#include "HttpClientImpl.h"
using namespace drogon;
HttpClientImpl::HttpClientImpl(trantor::EventLoop *loop,
                               const trantor::InetAddress &addr,
                               bool useSSL):
                                       _loop(loop),
                                       _server(addr),
                                       _useSSL(useSSL)
{

}

void HttpClientImpl::sendRequest(const HttpRequest &req,const HttpReqCallback &callback)
{
    bool newConn=false;
    {
        std::lock_guard<std::mutex> guard(_mutex);
        if(!_tcpClient)
        {
            _tcpClient=std::make_shared<trantor::TcpClient>(_loop,_server,"httpClient");
            newConn=true;
        }
    }
    if(newConn)
    {
#ifdef USE_OPENSSL
        if(_useSSL)
        {
            _tcpClient->enableSSL();
        }
#endif
        auto thisPtr=shared_from_this();
        _tcpClient->setConnectionCallback([=](const trantor::TcpConnectionPtr& connPtr){
            if(connPtr->connected())
            {
                //send request;
                //connPtr->send(req.)
            }
            else
            {
                std::lock_guard<std::mutex> guard(_mutex);
                thisPtr->_tcpClient.reset();
            }
        });
        _tcpClient->connect();

    }else
    {
        //send request;
    }

}