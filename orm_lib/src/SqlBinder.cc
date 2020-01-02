/**
 *
 *  SqlBinder.cc
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

#include <drogon/config.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/SqlBinder.h>
#include <future>
#include <iostream>
#include <stdio.h>
#if USE_MYSQL
#include <mysql.h>
#endif
using namespace drogon::orm;
using namespace drogon::orm::internal;
void SqlBinder::exec()
{
    execed_ = true;
    if (mode_ == Mode::NonBlocking)
    {
        // nonblocking mode,default mode
        // Retain shared_ptrs of parameters until we get the result;
        client_.execSql(
            sqlView_.data(),
            sqlView_.length(),
            parametersNumber_,
            std::move(parameters_),
            std::move(lengths_),
            std::move(formats_),
            [holder = std::move(callbackHolder_),
             objs = std::move(objs_),
             sqlptr = sqlPtr_](const Result &r) mutable {
                objs.clear();
                if (holder)
                {
                    holder->execCallback(r);
                }
            },
            [exceptCb = std::move(exceptionCallback_),
             exceptPtrCb = std::move(exceptionPtrCallback_),
             isExceptPtr = isExceptionPtr_,
             sqlptr = sqlPtr_](const std::exception_ptr &exception) {
                // LOG_DEBUG<<"exp callback "<<isExceptPtr;
                if (!isExceptPtr)
                {
                    if (exceptCb)
                    {
                        try
                        {
                            std::rethrow_exception(exception);
                        }
                        catch (const DrogonDbException &e)
                        {
                            exceptCb(e);
                        }
                    }
                }
                else
                {
                    if (exceptPtrCb)
                        exceptPtrCb(exception);
                }
            });
    }
    else
    {
        // blocking mode
        std::shared_ptr<std::promise<Result>> pro(new std::promise<Result>);
        auto f = pro->get_future();

        client_.execSql(
            sqlView_.data(),
            sqlView_.length(),
            parametersNumber_,
            std::move(parameters_),
            std::move(lengths_),
            std::move(formats_),
            [pro](const Result &r) { pro->set_value(r); },
            [pro](const std::exception_ptr &exception) {
                try
                {
                    pro->set_exception(exception);
                }
                catch (...)
                {
                    assert(0);
                }
            });

        try
        {
            const Result &v = f.get();
            if (callbackHolder_)
            {
                callbackHolder_->execCallback(v);
            }
        }
        catch (const DrogonDbException &exception)
        {
            if (!destructed_)
            {
                // throw exception
                std::rethrow_exception(std::current_exception());
            }
            else
            {
                if (exceptionCallback_)
                {
                    exceptionCallback_(exception);
                }
            }
        }
    }
}
SqlBinder::~SqlBinder()
{
    destructed_ = true;
    if (!execed_)
    {
        exec();
    }
}

SqlBinder &SqlBinder::operator<<(const std::string &str)
{
    std::shared_ptr<std::string> obj = std::make_shared<std::string>(str);
    objs_.push_back(obj);
    ++parametersNumber_;
    parameters_.push_back((char *)obj->c_str());
    lengths_.push_back(obj->length());
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(0);
    }
    else if (type_ == ClientType::Mysql)
    {
#if USE_MYSQL
        formats_.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (type_ == ClientType::Sqlite3)
    {
        formats_.push_back(Sqlite3TypeText);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(std::string &&str)
{
    std::shared_ptr<std::string> obj =
        std::make_shared<std::string>(std::move(str));
    objs_.push_back(obj);
    ++parametersNumber_;
    parameters_.push_back((char *)obj->c_str());
    lengths_.push_back(obj->length());
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(0);
    }
    else if (type_ == ClientType::Mysql)
    {
#if USE_MYSQL
        formats_.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (type_ == ClientType::Sqlite3)
    {
        formats_.push_back(Sqlite3TypeText);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(const std::vector<char> &v)
{
    std::shared_ptr<std::vector<char>> obj =
        std::make_shared<std::vector<char>>(v);
    objs_.push_back(obj);
    ++parametersNumber_;
    parameters_.push_back((char *)obj->data());
    lengths_.push_back(obj->size());
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(1);
    }
    else if (type_ == ClientType::Mysql)
    {
#if USE_MYSQL
        formats_.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (type_ == ClientType::Sqlite3)
    {
        formats_.push_back(Sqlite3TypeBlob);
    }
    return *this;
}
SqlBinder &SqlBinder::operator<<(std::vector<char> &&v)
{
    std::shared_ptr<std::vector<char>> obj =
        std::make_shared<std::vector<char>>(std::move(v));
    objs_.push_back(obj);
    ++parametersNumber_;
    parameters_.push_back((char *)obj->data());
    lengths_.push_back(obj->size());
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(1);
    }
    else if (type_ == ClientType::Mysql)
    {
#if USE_MYSQL
        formats_.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (type_ == ClientType::Sqlite3)
    {
        formats_.push_back(Sqlite3TypeBlob);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(double f)
{
    if (type_ == ClientType::Sqlite3)
    {
        ++parametersNumber_;
        auto obj = std::make_shared<double>(f);
        objs_.push_back(obj);
        formats_.push_back(Sqlite3TypeDouble);
        lengths_.push_back(0);
        parameters_.push_back((char *)(obj.get()));
        return *this;
    }
    return operator<<(std::to_string(f));
}
SqlBinder &SqlBinder::operator<<(std::nullptr_t nullp)
{
    (void)nullp;
    ++parametersNumber_;
    parameters_.push_back(NULL);
    lengths_.push_back(0);
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(0);
    }
    else if (type_ == ClientType::Mysql)
    {
#if USE_MYSQL
        formats_.push_back(MYSQL_TYPE_NULL);
#endif
    }
    else if (type_ == ClientType::Sqlite3)
    {
        formats_.push_back(Sqlite3TypeNull);
    }
    return *this;
}

int SqlBinder::getMysqlTypeBySize(size_t size)
{
#if USE_MYSQL
    switch (size)
    {
        case 1:
            return MYSQL_TYPE_TINY;
            break;
        case 2:
            return MYSQL_TYPE_SHORT;
            break;
        case 4:
            return MYSQL_TYPE_LONG;
            break;
        case 8:
            return MYSQL_TYPE_LONGLONG;
            break;
        default:
            return 0;
    }
#else
    LOG_FATAL << "Mysql is not supported!";
    exit(1);
    return 0;
#endif
}
