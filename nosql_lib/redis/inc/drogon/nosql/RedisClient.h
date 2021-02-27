/**
 *
 *  @file RedisClient.h
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
#pragma once

#include <drogon/nosql/RedisResult.h>
#include <drogon/nosql/RedisException.h>
#include <drogon/utils/string_view.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Logger.h>
#include <memory>
#include <functional>
#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif

namespace drogon
{
namespace nosql
{
#ifdef __cpp_impl_coroutine
class RedisClient;
class RedisTransaction;
namespace internal
{
struct RedisAwaiter : public CallbackAwaiter<RedisResult>
{
    using RedisFunction =
        std::function<void(RedisResultCallback &&, RedisExceptionCallback &&)>;
    explicit RedisAwaiter(RedisFunction &&function)
        : function_(std::move(function))
    {
    }
    void await_suspend(std::coroutine_handle<> handle)
    {
        function_(
            [handle, this](const RedisResult &result) {
                this->setValue(result);
                handle.resume();
            },
            [handle, this](const RedisException &e) {
                LOG_ERROR << e.what();
                this->setException(std::make_exception_ptr(e));
                handle.resume();
            });
    }

  private:
    RedisFunction function_;
};

struct RedisTrasactionAwaiter
    : public CallbackAwaiter<std::shared_ptr<RedisTransaction>>
{
    RedisTrasactionAwaiter(RedisClient *client) : client_(client)
    {
    }

    void await_suspend(std::coroutine_handle<> handle);

  private:
    RedisClient *client_;
};
}  // namespace internal
#endif

class RedisTransaction;
/**
 * @brief This class represents a redis client that contains several connections
 * to a redis server.
 *
 */
class RedisClient
{
  public:
    /**
     * @brief Create a new redis client with multiple connections;
     *
     * @param serverAddress The server address.
     * @param numberOfConnections The number of connections. 1 by default.
     * @param password The password to authenticate if necessary.
     * @return std::shared_ptr<RedisClient>
     */
    static std::shared_ptr<RedisClient> newRedisClient(
        const trantor::InetAddress &serverAddress,
        size_t numberOfConnections = 1,
        const std::string &password = "");
    /**
     * @brief Execute a redis command
     *
     * @param resultCallback The callback is called when a redis reply is
     * received successfully.
     * @param exceptionCallback The callback is called when an error occurs.
     * @note When a redis reply with REDIS_REPLY_ERROR code is received, this
     * callback is called.
     * @param command The command to be executed. the command string can contain
     * some placeholders for parameters, such as '%s', '%d', etc.
     * @param ... The command parameters.
     * For example:
     * @code
       redisClientPtr->execCommandAsync([](const RedisResult &r){
           std::cout << r.getStringForDisplaying() << std::endl;
       },[](const std::exception &err){
           std::cerr << err.what() << std::endl;
       }, "get %s", key.data());
       @endcode
     */
    virtual void execCommandAsync(RedisResultCallback &&resultCallback,
                                  RedisExceptionCallback &&exceptionCallback,
                                  string_view command,
                                  ...) noexcept = 0;

    /**
     * @brief Create a redis transaction object.
     *
     * @return std::shared_ptr<RedisTransaction>
     */
    virtual std::shared_ptr<RedisTransaction> newTransaction() = 0;

    /**
     * @brief Create a transaction object in asynchronous mode.
     *
     * @return std::shared_ptr<RedisTransaction>
     */
    virtual void newTransactionAsync(
        const std::function<void(const std::shared_ptr<RedisTransaction> &)>
            &callback) = 0;
    virtual ~RedisClient() = default;
#ifdef __cpp_impl_coroutine
    template <typename... Arguments>
    Task<RedisResult> execCommandCoro(string_view command, Arguments... args)
    {
        co_return co_await internal::RedisAwaiter(
            [command,
             this,
             args...](RedisResultCallback &&commandCallback,
                      RedisExceptionCallback &&exceptionCallback) {
                execCommandAsync(std::move(commandCallback),
                                 std::move(exceptionCallback),
                                 command,
                                 args...);
            });
    }
    Task<std::shared_ptr<RedisTransaction>> newTransactionCoro()
    {
        co_return co_await nosql::internal::RedisTrasactionAwaiter(this);
    }
#endif
};
class RedisTransaction : public RedisClient
{
  public:
    // virtual void cancel() = 0;
    virtual void execute(RedisResultCallback &&resultCallback,
                         RedisExceptionCallback &&exceptionCallback) = 0;
#ifdef __cpp_impl_coroutine
    Task<RedisResult> executeCoro()
    {
        co_return co_await internal::RedisAwaiter(
            [this](RedisResultCallback &&resultCallback,
                   RedisExceptionCallback &&exceptionCallback) {
                execute(std::move(resultCallback),
                        std::move(exceptionCallback));
            });
    }
#endif
};
using RedisClientPtr = std::shared_ptr<RedisClient>;
using RedisTransactionPtr = std::shared_ptr<RedisTransaction>;

#ifdef __cpp_impl_coroutine
inline void internal::RedisTrasactionAwaiter::await_suspend(
    std::coroutine_handle<> handle)
{
    assert(client_ != nullptr);
    client_->newTransactionAsync(
        [this, &handle](const std::shared_ptr<RedisTransaction> &transaction) {
            if (transaction == nullptr)
                setException(std::make_exception_ptr(
                    RedisException(RedisErrorCode::kInternalError,
                                   "Failed to create transaction")));
            else
                setValue(transaction);
            handle.resume();
        });
}
#endif
}  // namespace nosql
}  // namespace drogon