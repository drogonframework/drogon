#include "Http1xTransport.h"
#include "HttpResponseParser.h"

using namespace drogon;

Http1xTransport::Http1xTransport(trantor::TcpConnectionPtr connPtr,
                                 Version version,
                                 size_t *bytesSent,
                                 size_t *bytesReceived)
    : connPtr(connPtr),
      bytesSent_(bytesSent),
      bytesReceived_(bytesReceived),
      version_(version)
{
    connPtr->setContext(std::make_shared<HttpResponseParser>(connPtr));
}

void Http1xTransport::sendRequestInLoop(const HttpRequestPtr &req,
                                        HttpReqCallback &&callback)
{
    sendReq(req);
    pipeliningCallbacks_.emplace(std::move(req), std::move(callback));
}

void Http1xTransport::onRecvMessage(const trantor::TcpConnectionPtr &conn,
                                    trantor::MsgBuffer *msg)
{
    auto responseParser = connPtr->getContext<HttpResponseParser>();
    assert(responseParser != nullptr);
    assert(connPtr.get() == conn.get());

    // LOG_TRACE << "###:" << msg->readableBytes();
    auto msgSize = msg->readableBytes();
    while (msg->readableBytes() > 0)
    {
        if (pipeliningCallbacks_.empty())
        {
            LOG_ERROR << "More responses than expected!";
            connPtr->shutdown();
            return;
        }
        auto &firstReq = pipeliningCallbacks_.front();
        if (firstReq.first->method() == Head)
        {
            responseParser->setForHeadMethod();
        }
        if (!responseParser->parseResponse(msg))
        {
            *bytesReceived_ += (msgSize - msg->readableBytes());
            errorCallback(ReqResult::BadResponse);
            return;
        }
        if (responseParser->gotAll())
        {
            auto resp = responseParser->responseImpl();
            responseParser->reset();
            *bytesReceived_ += (msgSize - msg->readableBytes());
            msgSize = msg->readableBytes();
            respCallback(resp, std::move(firstReq), conn);

            pipeliningCallbacks_.pop();
        }
        else
        {
            *bytesReceived_ += (msgSize - msg->readableBytes());
            break;
        }
    }
}

Http1xTransport::~Http1xTransport()
{
}

bool Http1xTransport::handleConnectionClose()
{
    auto responseParser = connPtr->getContext<HttpResponseParser>();
    if (responseParser && responseParser->parseResponseOnClose() &&
        responseParser->gotAll())
    {
        auto &firstReq = pipeliningCallbacks_.front();
        if (firstReq.first->method() == Head)
        {
            responseParser->setForHeadMethod();
        }
        auto resp = responseParser->responseImpl();
        responseParser->reset();
        respCallback(resp, std::move(firstReq), connPtr);
        return false;
    }
    return true;
}

void Http1xTransport::sendReq(const HttpRequestPtr &req)
{
    trantor::MsgBuffer buffer;
    assert(req);
    auto implPtr = static_cast<HttpRequestImpl *>(req.get());
    assert(version_ == Version::kHttp10 || version_ == Version::kHttp11);
    implPtr->appendToBuffer(&buffer, version_);
    LOG_TRACE << "Send request:"
              << std::string_view(buffer.peek(), buffer.readableBytes());
    *bytesSent_ += buffer.readableBytes();
    connPtr->send(std::move(buffer));
}
