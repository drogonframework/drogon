/**
 *
 *  @file Sqlite3Connection.cc
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

#include "Sqlite3Connection.h"
#include "Sqlite3ResultImpl.h"
#include <drogon/utils/Utilities.h>
#include <drogon/utils/string_view.h>
#include <regex>
#include <cctype>

using namespace drogon;
using namespace drogon::orm;

std::once_flag Sqlite3Connection::once_;

void Sqlite3Connection::onError(
    const string_view &sql,
    const std::function<void(const std::exception_ptr &)> &exceptCallback)
{
    try
    {
        throw SqlError(sqlite3_errmsg(connectionPtr_.get()), std::string{sql});
    }
    catch (...)
    {
        auto exceptPtr = std::current_exception();
        exceptCallback(exceptPtr);
    }
}

Sqlite3Connection::Sqlite3Connection(
    trantor::EventLoop *loop,
    const std::string &connInfo,
    const std::shared_ptr<SharedMutex> &sharedMutex)
    : DbConnection(loop), sharedMutexPtr_(sharedMutex)
{
    loopThread_.run();
    loop_ = loopThread_.getLoop();
    std::call_once(once_, []() {
        auto ret = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
        if (ret != SQLITE_OK)
        {
            LOG_FATAL << "SQLITE_CONFIG_MULTITHREAD is not supported!";
        }
    });
    // Get the key and value
    auto connParams = parseConnString(connInfo);
    std::string filename;
    for (auto const &kv : connParams)
    {
        auto key = kv.first;
        auto value = kv.second;
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        if (key == "filename")
        {
            filename = value;
        }
    }
    loop_->runInLoop([this, filename = std::move(filename)]() {
        sqlite3 *tmp = nullptr;
        auto ret = sqlite3_open(filename.data(), &tmp);
        connectionPtr_ = std::shared_ptr<sqlite3>(tmp, [](sqlite3 *ptr) {
            sqlite3_close(ptr);
        });
        auto thisPtr = shared_from_this();
        if (ret != SQLITE_OK)
        {
            LOG_FATAL << sqlite3_errmsg(connectionPtr_.get());
            closeCallback_(thisPtr);
        }
        else
        {
            sqlite3_extended_result_codes(tmp, true);
            okCallback_(thisPtr);
        }
    });
}

void Sqlite3Connection::execSql(
    string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    auto thisPtr = shared_from_this();
    loopThread_.getLoop()->runInLoop(
        [thisPtr,
         sql = std::move(sql),
         paraNum,
         parameters = std::move(parameters),
         length = std::move(length),
         format = std::move(format),
         rcb = std::move(rcb),
         exceptCallback = std::move(exceptCallback)]() mutable {
            thisPtr->execSqlInQueue(
                sql, paraNum, parameters, length, format, rcb, exceptCallback);
        });
}

void Sqlite3Connection::execSqlInQueue(
    const string_view &sql,
    size_t paraNum,
    const std::vector<const char *> &parameters,
    const std::vector<int> &length,
    const std::vector<int> &format,
    const ResultCallback &rcb,
    const std::function<void(const std::exception_ptr &)> &exceptCallback)
{
    LOG_TRACE << "sql:" << sql;
    std::shared_ptr<sqlite3_stmt> stmtPtr;
    bool newStmt = false;
    if (paraNum > 0)
    {
        auto iter = stmtsMap_.find(sql);
        if (iter != stmtsMap_.end())
        {
            stmtPtr = iter->second;
        }
    }
    if (!stmtPtr)
    {
        sqlite3_stmt *stmt = nullptr;
        newStmt = true;
        const char *remaining;
        auto ret = sqlite3_prepare_v2(
            connectionPtr_.get(), sql.data(), -1, &stmt, &remaining);
        stmtPtr = stmt ? std::shared_ptr<sqlite3_stmt>(stmt,
                                                       [](sqlite3_stmt *p) {
                                                           sqlite3_finalize(p);
                                                       })
                       : nullptr;
        if (ret != SQLITE_OK || !stmtPtr)
        {
            onError(sql, exceptCallback);
            idleCb_();
            return;
        }
        if (!std::all_of(remaining, sql.data() + sql.size(), [](char ch) {
                return std::isspace(ch);
            }))
        {
            try
            {
                throw SqlError(
                    "Multiple semicolon separated statements are unsupported",
                    std::string{sql});
            }
            catch (...)
            {
                auto exceptPtr = std::current_exception();
                exceptCallback(exceptPtr);
            }
            idleCb_();
            return;
        }
    }
    assert(stmtPtr);
    auto stmt = stmtPtr.get();
    for (int i = 0; i < (int)parameters.size(); ++i)
    {
        int bindRet;
        switch (format[i])
        {
            case Sqlite3TypeChar:
                bindRet = sqlite3_bind_int(stmt, i + 1, *(char *)parameters[i]);
                break;
            case Sqlite3TypeShort:
                bindRet =
                    sqlite3_bind_int(stmt, i + 1, *(short *)parameters[i]);
                break;
            case Sqlite3TypeInt:
                bindRet =
                    sqlite3_bind_int(stmt, i + 1, *(int32_t *)parameters[i]);
                break;
            case Sqlite3TypeInt64:
                bindRet =
                    sqlite3_bind_int64(stmt, i + 1, *(int64_t *)parameters[i]);
                break;
            case Sqlite3TypeDouble:
                bindRet =
                    sqlite3_bind_double(stmt, i + 1, *(double *)parameters[i]);
                break;
            case Sqlite3TypeText:
                bindRet = sqlite3_bind_text(
                    stmt, i + 1, parameters[i], -1, SQLITE_STATIC);
                break;
            case Sqlite3TypeBlob:
                bindRet = sqlite3_bind_blob(
                    stmt, i + 1, parameters[i], length[i], SQLITE_STATIC);
                break;
            case Sqlite3TypeNull:
                bindRet = sqlite3_bind_null(stmt, i + 1);
                break;
        }
        if (bindRet != SQLITE_OK)
        {
            onError(sql, exceptCallback);
            sqlite3_reset(stmt);
            idleCb_();
            return;
        }
    }
    int r;
    int columnNum = sqlite3_column_count(stmt);
    auto resultPtr = std::make_shared<Sqlite3ResultImpl>();
    for (int i = 0; i < columnNum; ++i)
    {
        auto name = std::string(sqlite3_column_name(stmt, i));
        std::transform(name.begin(), name.end(), name.begin(), tolower);
        LOG_TRACE << "column name:" << name;
        resultPtr->columnNames_.push_back(name);
        resultPtr->columnNamesMap_.insert({name, i});
    }

    if (sqlite3_stmt_readonly(stmt))
    {
        // Readonly, hold read lock;
        std::shared_lock<SharedMutex> lock(*sharedMutexPtr_);
        r = stmtStep(stmt, resultPtr, columnNum);
        sqlite3_reset(stmt);
    }
    else
    {
        // Hold write lock
        std::unique_lock<SharedMutex> lock(*sharedMutexPtr_);
        r = stmtStep(stmt, resultPtr, columnNum);
        if (r == SQLITE_DONE)
        {
            resultPtr->affectedRows_ = sqlite3_changes(connectionPtr_.get());
            resultPtr->insertId_ =
                sqlite3_last_insert_rowid(connectionPtr_.get());
        }
        sqlite3_reset(stmt);
    }

    if (r != SQLITE_DONE)
    {
        onError(sql, exceptCallback);
        sqlite3_reset(stmt);
        idleCb_();
        return;
    }
    if (paraNum > 0 && newStmt)
    {
        auto r = stmts_.insert(std::string{sql});
        stmtsMap_[std::string{r.first->data(), r.first->length()}] = stmtPtr;
    }
    rcb(Result(resultPtr));
    idleCb_();
}

int Sqlite3Connection::stmtStep(
    sqlite3_stmt *stmt,
    const std::shared_ptr<Sqlite3ResultImpl> &resultPtr,
    int columnNum)
{
    int r;
    while ((r = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        std::vector<std::shared_ptr<std::string>> row;
        for (int i = 0; i < columnNum; ++i)
        {
            switch (sqlite3_column_type(stmt, i))
            {
                case SQLITE_INTEGER:
                    row.push_back(std::make_shared<std::string>(
                        std::to_string(sqlite3_column_int64(stmt, i))));
                    break;
                case SQLITE_FLOAT:
                    row.push_back(std::make_shared<std::string>(
                        std::to_string(sqlite3_column_double(stmt, i))));
                    break;
                case SQLITE_TEXT:
                    row.push_back(std::make_shared<std::string>(
                        (const char *)sqlite3_column_text(stmt, i),
                        (size_t)sqlite3_column_bytes(stmt, i)));
                    break;
                case SQLITE_BLOB:
                {
                    const char *buf =
                        (const char *)sqlite3_column_blob(stmt, i);
                    size_t len = sqlite3_column_bytes(stmt, i);
                    row.push_back(buf ? std::make_shared<std::string>(buf, len)
                                      : std::make_shared<std::string>());
                }
                break;
                case SQLITE_NULL:
                    row.push_back(nullptr);
                    break;
            }
        }
        resultPtr->result_.push_back(std::move(row));
    }
    return r;
}
void Sqlite3Connection::disconnect()
{
    std::promise<int> pro;
    auto f = pro.get_future();
    auto thisPtr = shared_from_this();
    loopThread_.getLoop()->runInLoop([thisPtr, &pro]() {
        thisPtr->connectionPtr_.reset();
        pro.set_value(1);
    });
    f.get();
}
