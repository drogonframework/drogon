#include <drogon/drogon.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <drogon/HttpTypes.h>
#include <trantor/utils/Logger.h>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif
#include <drogon/utils/Http11ClientPool.h>
using namespace drogon;

int nth_resp = 0;

int main()
{
    auto func = [](int fd) {
        std::cout << "setSockOptCallback:" << fd << std::endl;
#ifdef __linux__
        int optval = 10;
        ::setsockopt(fd,
                     SOL_TCP,
                     TCP_KEEPCNT,
                     &optval,
                     static_cast<socklen_t>(sizeof optval));
        ::setsockopt(fd,
                     SOL_TCP,
                     TCP_KEEPIDLE,
                     &optval,
                     static_cast<socklen_t>(sizeof optval));
        ::setsockopt(fd,
                     SOL_TCP,
                     TCP_KEEPINTVL,
                     &optval,
                     static_cast<socklen_t>(sizeof optval));
#endif
    };
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);
#ifdef __cpp_impl_coroutine
    Http11ClientPoolConfig cfg{
        .hostString = "http://www.baidu.com",
        .useOldTLS = false,
        .validateCert = false,
        .size = 10,
        .setCallback =
            [func](auto &client) {
                LOG_INFO << "setCallback";
                client->setSockOptCallback(func);
            },
        .numOfThreads = 4,
        .keepaliveRequests = 1000,
        .idleTimeout = std::chrono::seconds(10),
        .maxLifeTime = std::chrono::seconds(300),
        .checkInterval = std::chrono::seconds(10),
    };
    auto pool = std::make_unique<Http11ClientPool>(cfg);
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/s");
    req->setParameter("wd", "wx");
    req->setParameter("oq", "wx");

    for (int i = 0; i < 1; i++)
    {
        [](auto req, auto &pool) -> drogon::AsyncTask {
            {
                auto [result, resp] = co_await pool->sendRequestCoro(req, 10);
                if (result == ReqResult::Ok)
                    LOG_INFO << "1:" << resp->getStatusCode();
            }
            {
                auto [result, resp] = co_await pool->sendRequestCoro(req, 10);
                if (result == ReqResult::Ok)
                    LOG_INFO << "2:" << resp->getStatusCode();
            }
            {
                auto [result, resp] = co_await pool->sendRequestCoro(req, 10);
                if (result == ReqResult::Ok)
                    LOG_INFO << "3:" << resp->getStatusCode();
            }
            co_return;
        }(req, pool);
    }

    for (int i = 0; i < 10; i++)
    {
        pool->sendRequest(
            req,
            [](ReqResult result, const HttpResponsePtr &response) {
                if (result != ReqResult::Ok)
                {
                    LOG_ERROR
                        << "error while sending request to server! result: "
                        << result;
                    return;
                }
                LOG_INFO << "callback:" << response->getStatusCode();
            },
            10);
    }
    std::this_thread::sleep_for(std::chrono::seconds(30));
#else
    {
        auto client = HttpClient::newHttpClient("http://www.baidu.com");
        client->setSockOptCallback(func);

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/s");
        req->setParameter("wd", "wx");
        req->setParameter("oq", "wx");

        for (int i = 0; i < 10; ++i)
        {
            client->sendRequest(
                req, [](ReqResult result, const HttpResponsePtr &response) {
                    if (result != ReqResult::Ok)
                    {
                        std::cout
                            << "error while sending request to server! result: "
                            << result << std::endl;
                        return;
                    }

                    std::cout << "receive response!" << std::endl;
                    // auto headers=response.
                    ++nth_resp;
                    std::cout << response->getBody() << std::endl;
                    auto cookies = response->cookies();
                    for (auto const &cookie : cookies)
                    {
                        std::cout << cookie.first << "="
                                  << cookie.second.value()
                                  << ":domain=" << cookie.second.domain()
                                  << std::endl;
                    }
                    std::cout << "count=" << nth_resp << std::endl;
                });
        }
        std::cout << "requestsBufferSize:" << client->requestsBufferSize()
                  << std::endl;
    }

    app().run();
#endif
}
