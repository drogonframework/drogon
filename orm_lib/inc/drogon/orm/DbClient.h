/**
 *
 *  @file DbClient.h
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
#include <drogon/orm/Exception.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/ResultIterator.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/RowIterator.h>
#include <drogon/orm/SqlBinder.h>
#include <exception>
#include <functional>
#include <future>
#include <string>
#include <trantor/utils/Logger.h>
#include <trantor/utils/NonCopyable.h>

#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif

namespace drogon
{
namespace orm
{
using ResultCallback = std::function<void(const Result &)>;
using ExceptionCallback = std::function<void(const DrogonDbException &)>;

class Transaction;
class DbClient;

namespace internal
{
#ifdef __cpp_impl_coroutine
struct [[nodiscard]] SqlAwaiter : public CallbackAwaiter<Result>
{
    SqlAwaiter(internal::SqlBinder &&binder) : binder_(std::move(binder))
    {
    }

    void await_suspend(std::coroutine_handle<> handle)
    {
        binder_ >> [handle, this](const drogon::orm::Result &result) {
            setValue(result);
            handle.resume();
        };
        binder_ >> [handle, this](const std::exception_ptr &e) {
            setException(e);
            handle.resume();
        };
        binder_.exec();
    }

  private:
    internal::SqlBinder binder_;
};

struct [[nodiscard]] TransactionAwaiter
    : public CallbackAwaiter<std::shared_ptr<Transaction> >
{
    TransactionAwaiter(DbClient *client) : client_(client)
    {
    }

    void await_suspend(std::coroutine_handle<> handle);

  private:
    DbClient *client_;
};

#endif

}  // namespace internal

/// Database client abstract class
class DROGON_EXPORT DbClient : public trantor::NonCopyable
{
  public:
    virtual ~DbClient(){};
    /// Create a new database client with multiple connections;
    /**
     * @param connInfo: Connection string with some parameters,
     * each parameter setting is in the form keyword = value. Spaces around the
     * equal sign are optional.
     * To write an empty value, or a value containing spaces, surround it with
     * single quotes, e.g.,
     * keyword = 'a value'. Single quotes and backslashes within the value must
     * be escaped with a backslash, i.e., \' and \\. Example: host=localhost
     * port=5432 dbname=mydb connect_timeout=10 password='' The currently
     * recognized parameter key words are:
     * - host: can be either a host name or an IP address.
     * - port: Port number to connect to at the server host.
     * - dbname: The database name. Defaults to be the same as the user name.
     * - user:  user name to connect as. With PostgreSQL defaults to be the same
     * as the operating system name of the user running the application.
     * - password: Password to be used if the server demands password
     * authentication.
     * - client_encoding: The character set to be used on database connections.
     *
     * For other key words on PostgreSQL, see the PostgreSQL documentation.
     * Only a pair of key values is valid for Sqlite3, and its keyword is
     * 'filename'.
     *
     * @param connNum: The number of connections to database server;
     */
    static std::shared_ptr<DbClient> newPgClient(const std::string &connInfo,
                                                 const size_t connNum);
    static std::shared_ptr<DbClient> newMysqlClient(const std::string &connInfo,
                                                    const size_t connNum);
    static std::shared_ptr<DbClient> newSqlite3Client(
        const std::string &connInfo,
        const size_t connNum);

    /// Async and nonblocking method
    /**
     * @param sql is the SQL statement to be executed;
     * @param FUNCTION1 is usually the ResultCallback type;
     * @param FUNCTION2 is usually the ExceptionCallback type;
     * @param args are parameters that are bound to placeholders in the sql
     * parameter;
     *
     * @note
     *
     * If the number of args parameters is not zero, make sure that all criteria
     * in the sql parameter set by bind parameters, for example:
     *
     *   1. select * from users where user_id > 10 limit 10 offset 10; //Not
     * bad, no bind parameters are used.
     *   2. select * from users where user_id > ? limit ? offset ?; //Good,
     * fully use bind parameters.
     *   3. select * from users where user_id > ? limit ? offset 10; //Bad,
     * partially use bind parameters.
     *
     * Strictly speaking, try not to splice SQL statements dynamically, Instead,
     * use the constant sql string with placeholders and the bind parameters to
     * execute sql. This rule makes the sql execute faster and more securely,
     * and users should follow this rule when calling all methods of DbClient.
     *
     */
    template <typename FUNCTION1, typename FUNCTION2, typename... Arguments>
    void execSqlAsync(const std::string &sql,
                      FUNCTION1 &&rCallback,
                      FUNCTION2 &&exceptCallback,
                      Arguments &&...args) noexcept
    {
        auto binder = *this << sql;
        (void)std::initializer_list<int>{
            (binder << std::forward<Arguments>(args), 0)...};
        binder >> std::forward<FUNCTION1>(rCallback);
        binder >> std::forward<FUNCTION2>(exceptCallback);
    }

    /// Async and nonblocking method
    template <typename... Arguments>
    std::future<Result> execSqlAsyncFuture(const std::string &sql,
                                           Arguments &&...args) noexcept
    {
        auto binder = *this << sql;
        (void)std::initializer_list<int>{
            (binder << std::forward<Arguments>(args), 0)...};
        std::shared_ptr<std::promise<Result> > prom =
            std::make_shared<std::promise<Result> >();
        binder >> [prom](const Result &r) { prom->set_value(r); };
        binder >>
            [prom](const std::exception_ptr &e) { prom->set_exception(e); };
        binder.exec();
        return prom->get_future();
    }

    // Sync and blocking method
    template <typename... Arguments>
    const Result execSqlSync(const std::string &sql,
                             Arguments &&...args) noexcept(false)
    {
        Result r(nullptr);
        {
            auto binder = *this << sql;
            (void)std::initializer_list<int>{
                (binder << std::forward<Arguments>(args), 0)...};
            // Use blocking mode
            binder << Mode::Blocking;

            binder >> [&r](const Result &result) { r = result; };
            binder.exec();  // exec may be throw exception;
        }
        return r;
    }

#ifdef __cpp_impl_coroutine
    template <typename... Arguments>
    internal::SqlAwaiter execSqlCoro(const std::string &sql,
                                     Arguments &&...args) noexcept
    {
        auto binder = *this << sql;
        (void)std::initializer_list<int>{
            (binder << std::forward<Arguments>(args), 0)...};
        return internal::SqlAwaiter(std::move(binder));
    }
#endif

    /// Streaming-like method for sql execution. For more information, see the
    /// wiki page.
    internal::SqlBinder operator<<(const std::string &sql);
    internal::SqlBinder operator<<(std::string &&sql);
    template <int N>
    internal::SqlBinder operator<<(const char (&sql)[N])
    {
        return internal::SqlBinder(sql, N - 1, *this, type_);
    }
    internal::SqlBinder operator<<(const string_view &sql)
    {
        return internal::SqlBinder(sql.data(), sql.length(), *this, type_);
    }

    /// Create a transaction object.
    /**
     * @param commitCallback: the callback with which user can get the
     * submitting result, The Boolean type parameter in the callback function
     * indicates whether the transaction was submitted successfully.
     *
     * @note The callback only indicates the result of the 'commit' command,
     * which is the last step of the transaction. If the transaction has been
     * automatically or manually rolled back, the callback will never be
     * executed. You can also use the setCommitCallback() method of a
     * transaction object to set the callback.
     * @note A TimeoutError exception is thrown if the operation is timed out.
     */
    virtual std::shared_ptr<Transaction> newTransaction(
        const std::function<void(bool)> &commitCallback =
            std::function<void(bool)>()) noexcept(false) = 0;

    /// Create a transaction object in asynchronous mode.
    /**
     * @note An empty shared_ptr object is returned via the callback if the
     * operation is timed out.
     */
    virtual void newTransactionAsync(
        const std::function<void(const std::shared_ptr<Transaction> &)>
            &callback) = 0;

#ifdef __cpp_impl_coroutine
    orm::internal::TransactionAwaiter newTransactionCoro()
    {
        return orm::internal::TransactionAwaiter(this);
    }
#endif

    /**
     * @brief Check if there is a connection successfully established.
     *
     * @return true
     * @return false
     */
    virtual bool hasAvailableConnections() const noexcept = 0;

    ClientType type() const
    {
        return type_;
    }
    const std::string &connectionInfo()
    {
        return connectionInfo_;
    }

    /**
     * @brief Set the Timeout value of execution of a SQL.
     *
     * @param timeout in seconds, if the SQL result is not returned from the
     * server within the timeout, a TimeoutError exception with "SQL execution
     * timeout" string is generated and returned to the caller.
     * @note set the timeout value to zero or negative for no limit on time. The
     * default value is -1.0, this means there is no time limit if this method
     * is not called.
     */
    virtual void setTimeout(double timeout) = 0;

  private:
    friend internal::SqlBinder;
    virtual void execSql(
        const char *sql,
        size_t sqlLength,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback) = 0;

  protected:
    ClientType type_;
    std::string connectionInfo_;
};
using DbClientPtr = std::shared_ptr<DbClient>;

class Transaction : public DbClient
{
  public:
    virtual void rollback() = 0;
    // virtual void commit() = 0;
    virtual void setCommitCallback(
        const std::function<void(bool)> &commitCallback) = 0;
};

#ifdef __cpp_impl_coroutine
inline void internal::TransactionAwaiter::await_suspend(
    std::coroutine_handle<> handle)
{
    assert(client_ != nullptr);
    client_->newTransactionAsync(
        [this, handle](const std::shared_ptr<Transaction> &transaction) {
            if (transaction == nullptr)
                setException(std::make_exception_ptr(TimeoutError(
                    "Timeout, no connection available for transaction")));
            else
                setValue(transaction);
            handle.resume();
        });
}
#endif

}  // namespace orm
}  // namespace drogon
