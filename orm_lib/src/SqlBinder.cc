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
            [ holder = std::move(_callbackHolder), objs = std::move(_objs) ](const Result &r) mutable {
                objs.clear();
                if (holder)
                {
                    holder->execCallback(r);
                }
            },
            [ exceptCb = std::move(_exceptCallback), exceptPtrCb = std::move(_exceptPtrCallback), isExceptPtr = _isExceptPtr ](
                const std::exception_ptr &exception) {
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

        _client.execSql(std::move(_sql),
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
