/**
 *
 *  HttpClient.h
 *  Martin Chang
 *
 *  Copyright 2021, Martin Chang.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#ifdef __cpp_impl_coro
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>

using namespace drogon;

struct HttpRespAwaiter : public CallbackAwaiter<HttpResponsePtr>
{
    HttpRespAwaiter(HttpClient *client, HttpRequestPtr req, double timeout)
        : client_(client), req_(std::move(req)), timeout_(timeout)
    {
    }

    void await_suspend(std::coroutine_handle<> handle)
    {
        client_->sendRequest(
            req_,
            [handle = std::move(handle), this](ReqResult result,
                                               const HttpResponsePtr &resp) {
                if (result == ReqResult::Ok)
                {
                    setValue(resp);
                    handle.resume();
                }
                else
                {
                    std::string reason;
                    if (result == ReqResult::BadResponse)
                        reason = "BadResponse";
                    else if (result == ReqResult::NetworkFailure)
                        reason = "NetworkFailure";
                    else if (result == ReqResult::BadServerAddress)
                        reason = "BadServerAddress";
                    else if (result == ReqResult::Timeout)
                        reason = "Timeout";
                    setException(
                        std::make_exception_ptr(std::runtime_error(reason)));
                    handle.resume();
                }
            },
            timeout_);
    }

  private:
    HttpClient *client_;
    HttpRequestPtr req_;
    double timeout_;
};

Task<HttpResponsePtr> HttpClient::sendRequestCoro(HttpRequestPtr req,
                                                  double timeout)
{
    co_return co_await internal::HttpRespAwaiter(this, std::move(req), timeout);
}
#endif
