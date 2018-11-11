/**
 *
 *  DbClient.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#pragma once
#include <drogon/config.h>
#include <drogon/orm/Exception.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/NonCopyable.h>
#include <drogon/orm/SqlBinder.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/ResultIterator.h>
#include <drogon/orm/RowIterator.h>
#include <string>
#include <functional>
#include <exception>
#include <future>

namespace drogon
{
namespace orm
{

typedef std::function<void(const Result &)> ResultCallback;
typedef std::function<void(const DrogonDbException &)> ExceptionCallback;

class DbClient : public trantor::NonCopyable
{
  public:
    virtual ~DbClient(){};
#if USE_POSTGRESQL
    static std::shared_ptr<DbClient> newPgClient(const std::string &connInfo, const size_t connNum);
#endif
    //Async method, nonblocking by default;
    template <
        typename FUNCTION1,
        typename FUNCTION2,
        typename... Arguments>
    void execSqlAsync(const std::string &sql,
                      Arguments &&... args,
                      FUNCTION1 rCallback,
                      FUNCTION2 exceptCallback,
                      bool blocking = false) noexcept
    {
        auto binder = *this << sql;
        std::vector<int> v = {(binder << std::forward<Arguments>(args), 0)...};
        if (blocking)
        {
            binder << Mode::Blocking;
        }
        binder >> rCallback;
        binder >> exceptCallback;
    }
    //Async and nonblocking method
    template <typename... Arguments>
    std::future<const Result> execSqlAsync(const std::string &sql,
                                           Arguments &&... args) noexcept
    {
        auto binder = *this << sql;
        std::vector<int> v = {(binder << std::forward<Arguments>(args), 0)...};
        std::shared_ptr<std::promise<const Result>> prom = std::make_shared<std::promise<const Result>>();
        binder >> [=](const Result &r) {
            prom->set_value(r);
        };
        binder >> [=](const std::exception_ptr &e) {
            prom->set_exception(e);
        };
        binder.exec();
        return prom->get_future();
    }
    //Sync and blocking method
    template <typename... Arguments>
    const Result execSqlSync(const std::string &sql,
                             Arguments &&... args) noexcept(false)
    {
        Result r(nullptr);
        {
            auto binder = *this << sql;
            std::vector<int> v = {(binder << std::forward<Arguments>(args), 0)...};
            //Use blocking mode
            binder << Mode::Blocking;

            binder >> [&r](const Result &result) {
                r = result;
            };
            binder.exec(); //exec may be throw exception;
        }
        return r;
    }

    internal::SqlBinder operator<<(const std::string &sql);
    virtual std::string replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const = 0;

  private:
    friend internal::SqlBinder;
    virtual void execSql(const std::string &sql,
                         size_t paraNum,
                         const std::vector<const char *> &parameters,
                         const std::vector<int> &length,
                         const std::vector<int> &format,
                         const ResultCallback &rcb,
                         const std::function<void(const std::exception_ptr &)> &exptCallback) = 0;
};
typedef std::shared_ptr<DbClient> DbClientPtr;

} // namespace orm
} // namespace drogon