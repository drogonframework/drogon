/**
 *
 *  SqlBinder.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */
#include <drogon/config.h>
#include <drogon/orm/SqlBinder.h>
#include <drogon/orm/DbClient.h>
#include <stdio.h>
#include <future>
#include <iostream>
using namespace drogon::orm;
using namespace drogon::orm::internal;
void SqlBinder::exec()
{
    _execed = true;
    if (_mode == Mode::NonBlocking)
    {
        //nonblocking mode,default mode
        auto holder = std::move(_callbackHolder);
        auto exceptCb = std::move(_exceptCallback);
        auto exceptPtrCb = std::move(_exceptPtrCallback);
        auto isExceptPtr = _isExceptPtr;

        _client.execSql(_sql, _paraNum, _parameters, _length, _format,
                        [holder](const Result &r) {
                            if (holder)
                            {
                                holder->execCallback(r);
                            }
                        },
                        [exceptCb, exceptPtrCb, isExceptPtr](const std::exception_ptr &exception) {
                            //LOG_DEBUG<<"exp callback "<<isExceptPtr;
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
        //blocking mode
        std::shared_ptr<std::promise<Result>> pro(new std::promise<Result>);
        auto f = pro->get_future();

        _client.execSql(_sql, _paraNum, _parameters, _length, _format,
                        [pro](const Result &r) {
                            pro->set_value(r);
                        },
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
                    //throw exception
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
