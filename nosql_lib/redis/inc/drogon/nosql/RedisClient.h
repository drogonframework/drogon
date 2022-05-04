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

#include <drogon/exports.h>
#include <drogon/nosql/RedisResult.h>
#include <drogon/nosql/RedisException.h>
#include <drogon/nosql/RedisSubscriber.h>
#include <drogon/utils/string_view.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Logger.h>
#include <memory>
#include <functional>
#include <future>
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
struct [[nodiscard]] RedisAwaiter : public CallbackAwaiter<RedisResult>
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

struct [[nodiscard]] RedisTransactionAwaiter
    : public CallbackAwaiter<std::shared_ptr<RedisTransaction>>
{
    RedisTransactionAwaiter(RedisClient *client) : client_(client)
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
class DROGON_EXPORT RedisClient
{
  public:
    /**
     * @brief Create a new redis client with multiple connections;
     *
     * @param serverAddress The server address.
     * @param numberOfConnections The number of connections. 1 by default.
     * @param username The username to authenticate if necessary.
     * @param password The password to authenticate if necessary.
     * @return std::shared_ptr<RedisClient>
     */
    static std::shared_ptr<RedisClient> newRedisClient(
        const trantor::InetAddress &serverAddress,
        size_t numberOfConnections = 1,
        const std::string &password = "",
        unsigned int db = 0,
        const std::string &username = "");
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
     * @brief Execute a redis command synchronously
     *
     * @param processFunc Function to extract data from redis result.
     * received successfully.
     * @param command The command to be executed. the command string can contain
     * some placeholders for parameters, such as '%s', '%d', etc.
     * @param ... The command parameters.
     * @return Returns the same value as process callback.
     * For example:
     * @code
       try
       {
           std::string res = redisClientPtr->execCommandSync<std::string>(
               [](const RedisResult &r){
                   return r.asString();
               },
               "get %s",
               key.data()
           );
       }
       catch (const RedisException & err)
       {
       }
       catch (const std::exception & err)
       {
       }
       @endcode
     */
    template <typename T, typename... Args>
    T execCommandSync(std::function<T(const RedisResult &)> &&processFunc,
                      string_view command,
                      Args &&...args)
    {
        std::shared_ptr<std::promise<T>> pro(new std::promise<T>);
        std::future<T> f = pro->get_future();
        execCommandAsync(
            [process = std::move(processFunc), pro](const RedisResult &result) {
                try
                {
                    pro->set_value(process(result));
                }
                catch (...)
                {
                    pro->set_exception(std::current_exception());
                }
            },
            [pro](const RedisException &err) {
                pro->set_exception(std::make_exception_ptr(err));
            },
            command,
            std::forward<Args>(args)...);

        return f.get();
    }

    /**
     * @brief Create a subscriber for redis subscribe commands.
     *
     * @return std::shared_ptr<RedisSubscriber>
     * @note This subscriber creates a new redis connection dedicated to
     * subscribe commands. This connection is managed by RedisClient.
     */
    virtual std::shared_ptr<RedisSubscriber> newSubscriber() noexcept = 0;

    /**
     * @brief Create a redis transaction object.
     *
     * @return std::shared_ptr<RedisTransaction>
     * @note An exception with kTimeout code is thrown if the operation is
     * timed out. see RedisException.h
     */
    virtual std::shared_ptr<RedisTransaction> newTransaction() noexcept(
        false) = 0;

    /**
     * @brief Create a transaction object in asynchronous mode.
     *
     * @return std::shared_ptr<RedisTransaction>
     * @note An empty shared_ptr object is returned via the callback if the
     * operation is timed out.
     */
    virtual void newTransactionAsync(
        const std::function<void(const std::shared_ptr<RedisTransaction> &)>
            &callback) = 0;
    /**
     * @brief Set the Timeout value of execution of a command.
     *
     * @param timeout in seconds, if the result is not returned from the
     * server within the timeout, a RedisException with "Command execution
     * timeout" string is generated and returned to the caller.
     * @note set the timeout value to zero or negative for no limit on time.
     * The default value is -1.0, this means there is no time limit if this
     * method is not called.
     */
    virtual void setTimeout(double timeout) = 0;

    virtual ~RedisClient() = default;

    /**
     * @brief Close all connections in the client. usually used by Drogon in the
     * quit() method.
     * */
    virtual void closeAll() = 0;

#ifdef __cpp_impl_coroutine
    /**
     * @brief Send a Redis command and await the RedisResult in a coroutine.
     *
     * @tparam Arguments
     * @param command
     * @param args
     * @return internal::RedisAwaiter that can be awaited in a coroutine.
     * For example:
     * @code
        try
        {
            auto result = co_await redisClient->execCommandCoro("get %s",
     "keyname");
            std::cout << result.getStringForDisplaying() << "\n";
        }
        catch(const RedisException &err)
        {
            std::cout << err.what() << "\n";
        }
       @endcode
     */
    template <typename... Arguments>
    internal::RedisAwaiter execCommandCoro(string_view command,
                                           Arguments... args)
    {
        return internal::RedisAwaiter(
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
    /**
     * @brief await a RedisTransactionPtr in a coroutine.
     *
     * @return internal::RedisTransactionAwaiter that can be awaited in a
     * coroutine.
     * For example:
     * @code
        try
        {
            auto transPtr = co_await redisClient->newTransactionCoro();
            ...
        }
        catch(const RedisException &err)
        {
            std::cout << err.what() << "\n";
        }
       @endcode
     */
    internal::RedisTransactionAwaiter newTransactionCoro()
    {
        return internal::RedisTransactionAwaiter(this);
    }
#endif
};
class DROGON_EXPORT RedisTransaction : public RedisClient
{
  public:
    // virtual void cancel() = 0;
    virtual void execute(RedisResultCallback &&resultCallback,
                         RedisExceptionCallback &&exceptionCallback) = 0;
#ifdef __cpp_impl_coroutine
    /**
     * @brief Send a "exec" command to execute the transaction and await a
     * RedisResult in a coroutine.
     *
     * @return internal::RedisAwaiter that can be awaited in a coroutine.
     * For example:
     * @code
        try
        {
            auto transPtr = co_await redisClient->newTransactionCoro();
            ...
            auto result = co_await transPtr->executeCoro();
            std::cout << result.getStringForDisplaying() << "\n";
        }
        catch(const RedisException &err)
        {
            std::cout << err.what() << "\n";
        }
       @endcode
     */
    internal::RedisAwaiter executeCoro()
    {
        return internal::RedisAwaiter(
            [this](RedisResultCallback &&resultCallback,
                   RedisExceptionCallback &&exceptionCallback) {
                execute(std::move(resultCallback),
                        std::move(exceptionCallback));
            });
    }
#endif
    void closeAll() override
    {
    }
};
using RedisClientPtr = std::shared_ptr<RedisClient>;
using RedisTransactionPtr = std::shared_ptr<RedisTransaction>;

#ifdef __cpp_impl_coroutine
inline void internal::RedisTransactionAwaiter::await_suspend(
    std::coroutine_handle<> handle)
{
    assert(client_ != nullptr);
    client_->newTransactionAsync(
        [this, &handle](const std::shared_ptr<RedisTransaction> &transaction) {
            if (transaction == nullptr)
                setException(std::make_exception_ptr(RedisException(
                    RedisErrorCode::kTimeout,
                    "Timeout, no connection available for transaction")));
            else
                setValue(transaction);
            handle.resume();
        });
}
#endif
}  // namespace nosql
}  // namespace drogon
