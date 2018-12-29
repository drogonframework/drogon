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
#include <drogon/utils/Utilities.h>
#include <regex>

using namespace drogon::orm;

std::once_flag Sqlite3Connection::_once;

void Sqlite3Connection::onError(const std::string &sql, const std::function<void(const std::exception_ptr &)> &exceptCallback)
{
    try
    {
        throw SqlError(sqlite3_errmsg(_conn.get()), sql);
    }
    catch (...)
    {
        auto exceptPtr = std::current_exception();
        exceptCallback(exceptPtr);
    }
}

Sqlite3Connection::Sqlite3Connection(trantor::EventLoop *loop, const std::string &connInfo)
    : DbConnection(loop)
{
    _loopThread.run();
    _loop = _loopThread.getLoop();
    std::call_once(_once, []() {
        auto ret = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
        if (ret != SQLITE_OK)
        {
            LOG_FATAL << sqlite3_errstr(ret);
        }
    });
    //Get the key and value
    std::regex r(" *= *");
    auto tmpStr = std::regex_replace(connInfo, r, "=");
    std::string host, user, passwd, dbname, port;
    auto keyValues = splitString(tmpStr, " ");
    std::string filename;
    for (auto &kvs : keyValues)
    {
        auto kv = splitString(kvs, "=");
        assert(kv.size() == 2);
        auto key = kv[0];
        auto value = kv[1];
        if (value[0] == '\'' && value[value.length() - 1] == '\'')
        {
            value = value.substr(1, value.length() - 2);
        }
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        if (key == "filename")
        {
            filename = value;
        }
    }
    _loop->runInLoop([this, filename = std::move(filename)]() {
        sqlite3 *tmp = nullptr;
        auto ret = sqlite3_open(filename.data(), &tmp);
        _conn = std::shared_ptr<sqlite3>(tmp, [=](sqlite3 *ptr) { sqlite3_close(ptr); });
        auto thisPtr = shared_from_this();
        if (ret != SQLITE_OK)
        {
            LOG_FATAL << sqlite3_errstr(ret);
            _closeCb(thisPtr);
        }
        else
        {
            sqlite3_extended_result_codes(tmp, true);
            _okCb(thisPtr);
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
    _loopThread.getLoop()->runInLoop([=]() {
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
    LOG_TRACE << "sql:" << sql;
    sqlite3_stmt *stmt = nullptr;
    const char *remaining;

    auto ret = sqlite3_prepare_v2(_conn.get(), sql.data(), -1, &stmt, &remaining);
    std::shared_ptr<sqlite3_stmt> stmtPtr = stmt ? std::shared_ptr<sqlite3_stmt>(stmt,
                                                                                 [](sqlite3_stmt *p) {
                                                                                     sqlite3_finalize(p);
                                                                                 })
                                                 : nullptr;

    if (ret != SQLITE_OK)
    {
        onError(sql, exceptCallback);
        return;
    }
    if (!std::all_of(remaining, sql.data() + sql.size(), [](char ch) { return std::isspace(ch); }))
    {
        try
        {
            throw SqlError("Multiple semicolon separated statements are unsupported", sql);
        }
        catch (...)
        {
            auto exceptPtr = std::current_exception();
            exceptCallback(exceptPtr);
        }
        return;
    }
    for (int i = 0; i < (int)parameters.size(); i++)
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
            onError(sql, exceptCallback);
            return;
        }
    }
    int r;
    int columnNum = sqlite3_column_count(stmt);
    auto resultPtr = std::make_shared<Sqlite3ResultImpl>(sql);
    for (int i = 0; i < columnNum; i++)
    {
        auto name = std::string(sqlite3_column_name(stmt, i));
        std::transform(name.begin(), name.end(), name.begin(), tolower);
        LOG_TRACE << "column name:" << name;
        resultPtr->_columnNames.push_back(name);
        resultPtr->_columnNameMap.insert({name, i});
    }
    while ((r = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        std::vector<std::shared_ptr<std::string>> row;
        for (int i = 0; i < columnNum; i++)
        {

            switch (sqlite3_column_type(stmt, i))
            {
            case SQLITE_INTEGER:
                row.push_back(std::make_shared<std::string>(std::to_string(sqlite3_column_int64(stmt, i))));
                break;
            case SQLITE_FLOAT:
                row.push_back(std::make_shared<std::string>(std::to_string(sqlite3_column_double(stmt, i))));
                break;
            case SQLITE_TEXT:
                row.push_back(std::make_shared<std::string>((const char *)sqlite3_column_text(stmt, i), (size_t)sqlite3_column_bytes(stmt, i)));
                break;
            case SQLITE_BLOB:
            {
                const char *buf = (const char *)sqlite3_column_blob(stmt, i);
                size_t len = sqlite3_column_bytes(stmt, i);
                row.push_back(buf ? std::make_shared<std::string>(buf, len) : std::make_shared<std::string>());
            }
            break;
            case SQLITE_NULL:
                row.push_back(nullptr);
                break;
            }
        }
        resultPtr->_result.push_back(std::move(row));
    }
    if (r != SQLITE_DONE)
    {
        onError(sql, exceptCallback);
        return;
    }
    // If the sql is a select statement? FIXME
    resultPtr->_affectedRows = sqlite3_changes(_conn.get());
    resultPtr->_insertId = sqlite3_last_insert_rowid(_conn.get());
    // sqlite3_set_last_insert_rowid(_conn.get(), 0);
    rcb(Result(resultPtr));
    idleCb();
}
