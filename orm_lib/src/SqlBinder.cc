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
#include <drogon/utils/Utilities.h>
#include <future>
#include <regex>
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
            sqlViewPtr_,
            sqlViewLength_,
            parametersNumber_,
            std::move(parameters_),
            std::move(lengths_),
            std::move(formats_),
            [holder = std::move(callbackHolder_),
             objs = std::move(objs_),
             sqlptr = std::move(sqlPtr_)](const Result &r) mutable {
                objs.clear();
                if (holder)
                {
                    holder->execCallback(r);
                }
            },
            [exceptCb = std::move(exceptionCallback_),
             exceptPtrCb = std::move(exceptionPtrCallback_),
             isExceptPtr =
                 isExceptionPtr_](const std::exception_ptr &exception) {
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
            sqlViewPtr_,
            sqlViewLength_,
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

SqlBinder &SqlBinder::operator<<(const std::string_view &str)
{
    auto obj = std::make_shared<std::string>(str.data(), str.length());
    parameters_.push_back(obj->data());
    lengths_.push_back(static_cast<int>(obj->length()));
    objs_.push_back(obj);
    ++parametersNumber_;
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(0);
    }
    else if (type_ == ClientType::Mysql)
    {
        formats_.push_back(MySqlString);
    }
    else if (type_ == ClientType::Sqlite3)
    {
        formats_.push_back(Sqlite3TypeText);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(const std::string &str)
{
    auto obj = std::make_shared<std::string>(str);
    objs_.push_back(obj);
    ++parametersNumber_;
    parameters_.push_back(obj->data());
    lengths_.push_back(static_cast<int>(obj->length()));
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(0);
    }
    else if (type_ == ClientType::Mysql)
    {
        formats_.push_back(MySqlString);
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
    lengths_.push_back(static_cast<int>(obj->length()));
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(0);
    }
    else if (type_ == ClientType::Mysql)
    {
        formats_.push_back(MySqlString);
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
    lengths_.push_back(static_cast<int>(obj->size()));
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(1);
    }
    else if (type_ == ClientType::Mysql)
    {
        formats_.push_back(MySqlString);
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
    lengths_.push_back(static_cast<int>(obj->size()));
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(1);
    }
    else if (type_ == ClientType::Mysql)
    {
        formats_.push_back(MySqlString);
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

SqlBinder &SqlBinder::operator<<(std::nullptr_t)
{
    ++parametersNumber_;
    parameters_.push_back(nullptr);
    lengths_.push_back(0);
    if (type_ == ClientType::PostgreSQL)
    {
        formats_.push_back(0);
    }
    else if (type_ == ClientType::Mysql)
    {
        formats_.push_back(MySqlNull);
    }
    else if (type_ == ClientType::Sqlite3)
    {
        formats_.push_back(Sqlite3TypeNull);
    }
    return *this;
}

SqlBinder &SqlBinder::operator<<(DefaultValue dv)
{
    (void)dv;
    if (type_ == ClientType::PostgreSQL)
    {
        std::regex r("\\$" + std::to_string(parametersNumber_ + 1) + "\\b");
        // initialize with empty, as the next line will make a copy anyway
        if (!sqlPtr_)
            sqlPtr_ = std::make_shared<std::string>();

        *sqlPtr_ = std::regex_replace(sqlViewPtr_, r, "default");

        // decrement all other $n parameters by 1
        size_t i = parametersNumber_ + 2;
        while ((sqlPtr_->find("$" + std::to_string(i))) != std::string::npos)
        {
            r = "\\$" + std::to_string(i) + "\\b";
            // use sed format to avoid $n regex group substitution,
            // and use ->data() to compile in C++14 mode
            *sqlPtr_ = std::regex_replace(sqlPtr_->data(),
                                          r,
                                          "$" + std::to_string(i - 1),
                                          std::regex_constants::format_sed);
            ++i;
        }
        sqlViewPtr_ = sqlPtr_->data();
        sqlViewLength_ = sqlPtr_->length();
    }
    else if (type_ == ClientType::Mysql)
    {
        ++parametersNumber_;
        parameters_.push_back(nullptr);
        lengths_.push_back(0);
        formats_.push_back(DrogonDefaultValue);
    }
    else if (type_ == ClientType::Sqlite3)
    {
        LOG_FATAL << "default not supported in sqlite3";
        exit(1);
    }
    return *this;
}

int SqlBinder::getMysqlTypeBySize(size_t size)
{
#if USE_MYSQL
    switch (size)
    {
        case 1:
            return MySqlTiny;
            break;
        case 2:
            return MySqlShort;
            break;
        case 4:
            return MySqlLong;
            break;
        case 8:
            return MySqlLongLong;
            break;
        default:
            return 0;
    }
#else
    static_cast<void>(size);
    LOG_FATAL << "Mysql is not supported!";
    exit(1);
#endif
}
