/**
 *
 *  DbClient.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "DbClientImpl.h"
#include <drogon/config.h>
#include <drogon/orm/DbClient.h>
#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>
#endif
using namespace drogon::orm;
using namespace drogon;

internal::SqlBinder DbClient::operator<<(const std::string &sql)
{
    return internal::SqlBinder(sql, *this, type_);
}

internal::SqlBinder DbClient::operator<<(std::string &&sql)
{
    return internal::SqlBinder(std::move(sql), *this, type_);
}

std::shared_ptr<DbClient> DbClient::newPgClient(const std::string &connInfo,
                                                const size_t connNum)
{
#if USE_POSTGRESQL
    return std::make_shared<DbClientImpl>(connInfo,
                                          connNum,
                                          ClientType::PostgreSQL);
#else
    LOG_FATAL << "PostgreSQL is not supported!";
    exit(1);
#endif
}

std::shared_ptr<DbClient> DbClient::newMysqlClient(const std::string &connInfo,
                                                   const size_t connNum)
{
#if USE_MYSQL
    return std::make_shared<DbClientImpl>(connInfo, connNum, ClientType::Mysql);
#else
    LOG_FATAL << "Mysql is not supported!";
    exit(1);
#endif
}

std::shared_ptr<DbClient> DbClient::newSqlite3Client(
    const std::string &connInfo,
    const size_t connNum)
{
#if USE_SQLITE3
    return std::make_shared<DbClientImpl>(connInfo,
                                          connNum,
                                          ClientType::Sqlite3);
#else
    LOG_FATAL << "Sqlite3 is not supported!";
    exit(1);
#endif
}

#ifdef __cpp_impl_coroutine

struct TrasactionAwaiter : public CallbackAwaiter<std::shared_ptr<Transaction>>
{
    TrasactionAwaiter(DbClient *client) : client_(client)
    {
    }

    void await_suspend(std::coroutine_handle<> handle)
    {
        assert(client_ != nullptr);
        client_->newTransactionAsync(
            [this](const std::shared_ptr<Transaction> transacton) {
                if (transacton == nullptr)
                    setException(std::make_exception_ptr(
                        std::runtime_error("Failed to create transaction")));
                else
                    setValue(transacton);
                handle.resume();
            });
    }

  private:
    DbClient *client_;
};

Task<std::shared_ptr<Transaction>> DbClient::newTransactionCoro()
{
    co_return co_await TrasactionAwaiter(this);
}
#endif
