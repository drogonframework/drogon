/**
 *
 *  Sqlite3Connection.cc
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

#include "Sqlite3Connection.h"
#include "Sqlite3ResultImpl.h"
using namespace drogon::orm;

std::once_flag Sqlite3Connection::_once;

Sqlite3Connection::Sqlite3Connection(trantor::EventLoop *loop, const std::string &connInfo)
    : DbConnection(loop),
      _queue("Sqlite")
{
    std::call_once(_once, []() {
        auto ret = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
        if (ret != SQLITE_OK)
        {
            LOG_FATAL << sqlite3_errstr(ret);
        }
    });
    auto thisPtr = shared_from_this();
    _queue.runTaskInQueue([thisPtr, connInfo]() {
        sqlite3 *tmp = nullptr;
        auto ret = sqlite3_open(connInfo.data(), &tmp);
        thisPtr->_conn = std::shared_ptr<sqlite3>(tmp, [=](sqlite3 *ptr) { sqlite3_close(ptr); });

        if (ret != SQLITE_OK)
        {
            LOG_FATAL << sqlite3_errstr(ret);
            thisPtr->_closeCb(thisPtr);
        }
        else
        {
            sqlite3_extended_result_codes(tmp, true);
            thisPtr->_okCb(thisPtr);
        }
    });
}

void Sqlite3Connection::execSql(const std::string &sql,
                                size_t paraNum,
                                const std::vector<const char *> &parameters,
                                const std::vector<int> &length,
                                const std::vector<int> &format,
                                const ResultCallback &rcb,
                                const std::function<void(const std::exception_ptr &)> &exceptCallback,
                                const std::function<void()> &idleCb)
{
    auto thisPtr = shared_from_this();
    _queue.runTaskInQueue([=]() {
        thisPtr->execSqlInQueue(sql, paraNum, parameters, length, format, rcb, exceptCallback, idleCb);
    });
}

void Sqlite3Connection::execSqlInQueue(const std::string &sql,
                                       size_t paraNum,
                                       const std::vector<const char *> &parameters,
                                       const std::vector<int> &length,
                                       const std::vector<int> &format,
                                       const ResultCallback &rcb,
                                       const std::function<void(const std::exception_ptr &)> &exceptCallback,
                                       const std::function<void()> &idleCb)
{
    sqlite3_stmt *stmt = nullptr;
    const char *remaining;
    auto ret = sqlite3_prepare_v2(_conn.get(), sql.data(), -1, &stmt, &remaining);
    if (ret != SQLITE_OK)
    {
        //FIXME:throw exception here;
        return;
    }
    if (!std::all_of(remaining, sql.data() + sql.size(), [](char ch) { return std::isspace(ch); }))
    {
        ///close stmt
        ///FIXME 
        ///throw errors::more_statements("Multiple semicolon separated statements are unsupported", sql);
        return;
    }
    for (int i = 0; i < parameters.size(); i++)
    {
        int bindRet;
        switch (format[i])
        {
        case Sqlite3TypeChar:
            bindRet = sqlite3_bind_int(stmt, i + 1, *(char *)parameters[i]);
            break;
        case Sqlite3TypeShort:
            bindRet = sqlite3_bind_int(stmt, i + 1, *(short *)parameters[i]);
            break;
        case Sqlite3TypeInt:
            bindRet = sqlite3_bind_int(stmt, i + 1, *(int32_t *)parameters[i]);
            break;
        case Sqlite3TypeInt64:
            bindRet = sqlite3_bind_int64(stmt, i + 1, *(int64_t *)parameters[i]);
            break;
        case Sqlite3TypeDouble:
            bindRet = sqlite3_bind_double(stmt, i + 1, *(double *)parameters[i]);
            break;
        case Sqlite3TypeText:
            bindRet = sqlite3_bind_text(stmt, i + 1, parameters[i], -1, SQLITE_STATIC);
            break;
        case Sqlite3TypeBlob:
            bindRet = sqlite3_bind_blob(stmt, i + 1, parameters[i], length[i], SQLITE_STATIC);
            break;
        case Sqlite3TypeNull:
            bindRet = sqlite3_bind_null(stmt, i + 1);
            break;
        }
        if (bindRet != SQLITE_OK)
        {
            //FIXME throw!
            return;
        }
    }
    int r;
    int columnNum = sqlite3_column_count(stmt);
    Sqlite3ResultImpl result(sql);
    while ((r = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        std::vector<std::shared_ptr<std::string>> row;
        for (int i = 0; i < columnNum; i++)
        {

            switch (sqlite3_column_type(stmt, i))
            {
            case SQLITE_INTEGER:
            case SQLITE_FLOAT:
            case SQLITE_TEXT:
            case SQLITE_BLOB:
            case SQLITE_NULL:
                row.push_back(nullptr);
                break;
            }
        }
        result._result->push_back(std::move(row));
    }
    if (r != SQLITE_DONE)
    {
        //FIXME throw
        return;
    }
    rcb(result);
    idleCb();
}
