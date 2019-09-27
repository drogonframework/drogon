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
    _execed = true;
    if (_mode == Mode::NonBlocking)
    {
        // nonblocking mode,default mode
        // Retain shared_ptrs of parameters until we get the result;
        _client.execSql(
            std::move(_sql),
            _paraNum,
            std::move(_parameters),
            std::move(_length),
            std::move(_format),
            [holder = std::move(_callbackHolder),
             objs = std::move(_objs)](const Result &r) mutable {
                objs.clear();
                if (holder)
                {
                    holder->execCallback(r);
                }
            },
            [exceptCb = std::move(_exceptCallback),
             exceptPtrCb = std::move(_exceptPtrCallback),
             isExceptPtr = _isExceptPtr](const std::exception_ptr &exception) {
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

        _client.execSql(
            std::move(_sql),
            _paraNum,
            std::move(_parameters),
            std::move(_length),
            std::move(_format),
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
        if (_callbackHolder || _exceptCallback)
        {
            try
            {
                const Result &v = f.get();
                if (_callbackHolder)
                {
                    _callbackHolder->execCallback(v);
                }
            }
            catch (const DrogonDbException &exception)
            {
                if (!_destructed)
                {
                    // throw exception
                    std::rethrow_exception(std::current_exception());
                }
                else
                {
                    if (_exceptCallback)
                    {
                        _exceptCallback(exception);
                    }
                }
            }
        }
    }
}
SqlBinder::~SqlBinder()
{
    _destructed = true;
    if (!_execed)
    {
        exec();
    }
}

SqlBinder &SqlBinder::operator<<(const std::string &str)
{
    std::shared_ptr<std::string> obj = std::make_shared<std::string>(str);
    _objs.push_back(obj);
    _paraNum++;
    _parameters.push_back((char *)obj->c_str());
    _length.push_back(obj->length());
    if (_type == ClientType::PostgreSQL)
    {
        _format.push_back(0);
    }
    else if (_type == ClientType::Mysql)
    {
#if USE_MYSQL
        _format.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (_type == ClientType::Sqlite3)
    {
        _format.push_back(Sqlite3TypeText);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(std::string &&str)
{
    std::shared_ptr<std::string> obj =
        std::make_shared<std::string>(std::move(str));
    _objs.push_back(obj);
    _paraNum++;
    _parameters.push_back((char *)obj->c_str());
    _length.push_back(obj->length());
    if (_type == ClientType::PostgreSQL)
    {
        _format.push_back(0);
    }
    else if (_type == ClientType::Mysql)
    {
#if USE_MYSQL
        _format.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (_type == ClientType::Sqlite3)
    {
        _format.push_back(Sqlite3TypeText);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(const std::vector<char> &v)
{
    std::shared_ptr<std::vector<char>> obj =
        std::make_shared<std::vector<char>>(v);
    _objs.push_back(obj);
    _paraNum++;
    _parameters.push_back((char *)obj->data());
    _length.push_back(obj->size());
    if (_type == ClientType::PostgreSQL)
    {
        _format.push_back(1);
    }
    else if (_type == ClientType::Mysql)
    {
#if USE_MYSQL
        _format.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (_type == ClientType::Sqlite3)
    {
        _format.push_back(Sqlite3TypeBlob);
    }
    return *this;
}
SqlBinder &SqlBinder::operator<<(std::vector<char> &&v)
{
    std::shared_ptr<std::vector<char>> obj =
        std::make_shared<std::vector<char>>(std::move(v));
    _objs.push_back(obj);
    _paraNum++;
    _parameters.push_back((char *)obj->data());
    _length.push_back(obj->size());
    if (_type == ClientType::PostgreSQL)
    {
        _format.push_back(1);
    }
    else if (_type == ClientType::Mysql)
    {
#if USE_MYSQL
        _format.push_back(MYSQL_TYPE_STRING);
#endif
    }
    else if (_type == ClientType::Sqlite3)
    {
        _format.push_back(Sqlite3TypeBlob);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(double f)
{
    if (_type == ClientType::Sqlite3)
    {
        _paraNum++;
        auto obj = std::make_shared<double>(f);
        _objs.push_back(obj);
        _format.push_back(Sqlite3TypeDouble);
        _length.push_back(0);
        _parameters.push_back((char *)(obj.get()));
        return *this;
    }
    return operator<<(std::to_string(f));
}
SqlBinder &SqlBinder::operator<<(std::nullptr_t nullp)
{
    (void)nullp;
    _paraNum++;
    _parameters.push_back(NULL);
    _length.push_back(0);
    if (_type == ClientType::PostgreSQL)
    {
        _format.push_back(0);
    }
    else if (_type == ClientType::Mysql)
    {
#if USE_MYSQL
        _format.push_back(MYSQL_TYPE_NULL);
#endif
    }
    else if (_type == ClientType::Sqlite3)
    {
        _format.push_back(Sqlite3TypeNull);
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
