/**
 *
 *  @file DuckdbConnection.cc
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

#include "DuckdbConnection.h"
#include "DuckdbResultImpl.h"
#include <drogon/orm/Exception.h>
#include <drogon/utils/Utilities.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <cctype>
#include <exception>
#include <mutex>
#include <shared_mutex>

using namespace drogon;
using namespace drogon::orm;

std::once_flag DuckdbConnection::once_;

void DuckdbConnection::onError(
    const std::string_view &sql,
    const std::function<void(const std::exception_ptr &)> &exceptCallback,
    const char *errorMessage)
{
    // DuckDB 错误处理
    auto exceptPtr = std::make_exception_ptr(
        SqlError(errorMessage ? errorMessage : "Unknown DuckDB error",
                 std::string{sql}));
    exceptCallback(exceptPtr);
}

DuckdbConnection::DuckdbConnection(
    trantor::EventLoop *loop,
    const std::string &connInfo,
    const std::shared_ptr<SharedMutex> &sharedMutex)
    : DbConnection(loop), sharedMutexPtr_(sharedMutex), connInfo_(connInfo)
{
}

void DuckdbConnection::init()
{
    loopThread_.run();
    loop_ = loopThread_.getLoop();

    // 解析连接字符串
    auto connParams = parseConnString(connInfo_);
    std::string filename = ":memory:";  // 默认为内存数据库

    for (auto const &kv : connParams)
    {
        auto key = kv.first;
        auto value = kv.second;
        std::transform(key.begin(),
                       key.end(),
                       key.begin(),
                       [](unsigned char c) { return tolower(c); });
        if (key == "filename" || key == "dbname" || key == "path")
        {
            filename = value;
        }
    }

    loop_->runInLoop([this, filename = std::move(filename)]() {
        duckdb_database db;
        duckdb_connection conn;

        // 打开数据库
        auto state = duckdb_open(filename.c_str(), &db);
        if (state == DuckDBError)
        {
            LOG_FATAL << "Failed to open DuckDB database: " << filename;
            auto thisPtr = shared_from_this();
            closeCallback_(thisPtr);
            return;
        }

        // 创建连接
        state = duckdb_connect(db, &conn);
        if (state == DuckDBError)
        {
            LOG_FATAL << "Failed to connect to DuckDB database";
            duckdb_close(&db);
            auto thisPtr = shared_from_this();
            closeCallback_(thisPtr);
            return;
        }

        // 使用智能指针管理资源
        databasePtr_ = std::shared_ptr<duckdb_database>(
            new duckdb_database(db),
            [](duckdb_database *ptr) {
                if (ptr)
                {
                    duckdb_close(ptr);
                    delete ptr;
                }
            });

        connectionPtr_ = std::shared_ptr<duckdb_connection>(
            new duckdb_connection(conn),
            [](duckdb_connection *ptr) {
                if (ptr)
                {
                    duckdb_disconnect(ptr);
                    delete ptr;
                }
            });

        auto thisPtr = shared_from_this();
        okCallback_(thisPtr);
    });
}

void DuckdbConnection::execSql(
    std::string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    auto thisPtr = shared_from_this();
    loopThread_.getLoop()->queueInLoop(
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

void DuckdbConnection::execSqlInQueue(
    const std::string_view &sql,
    size_t paraNum,
    const std::vector<const char *> &parameters,
    const std::vector<int> &length,
    const std::vector<int> &format,
    const ResultCallback &rcb,
    const std::function<void(const std::exception_ptr &)> &exceptCallback)
{
    LOG_TRACE << "DuckDB sql:" << sql;

    std::shared_ptr<duckdb_prepared_statement> stmtPtr;
    bool newStmt = false;

    // 尝试从缓存中获取预编译语句
    if (paraNum > 0)
    {
        auto iter = stmtsMap_.find(sql);
        if (iter != stmtsMap_.end())
        {
            stmtPtr = iter->second;
        }
    }

    // 如果没有缓存，则准备新的语句
    if (!stmtPtr)
    {
        duckdb_prepared_statement stmt;
        auto state =
            duckdb_prepare(*connectionPtr_, sql.data(), &stmt);

        if (state == DuckDBError)
        {
            const char *error = duckdb_prepare_error(stmt);
            onError(sql, exceptCallback, error);
            duckdb_destroy_prepare(&stmt);
            idleCb_();
            return;
        }

        stmtPtr = std::shared_ptr<duckdb_prepared_statement>(
            new duckdb_prepared_statement(stmt),
            [](duckdb_prepared_statement *p) {
                if (p)
                {
                    duckdb_destroy_prepare(p);
                    delete p;
                }
            });
        newStmt = true;
    }

    assert(stmtPtr);
    auto stmt = *stmtPtr;

    // 绑定参数
    for (int i = 0; i < (int)parameters.size(); ++i)
    {
        duckdb_state bindRet = DuckDBSuccess;

        switch (format[i])
        {
            case Sqlite3TypeChar:
                bindRet = duckdb_bind_int8(stmt, i + 1, *(int8_t *)parameters[i]);
                break;
            case Sqlite3TypeShort:
                bindRet = duckdb_bind_int16(stmt, i + 1, *(int16_t *)parameters[i]);
                break;
            case Sqlite3TypeInt:
                bindRet = duckdb_bind_int32(stmt, i + 1, *(int32_t *)parameters[i]);
                break;
            case Sqlite3TypeInt64:
                bindRet = duckdb_bind_int64(stmt, i + 1, *(int64_t *)parameters[i]);
                break;
            case Sqlite3TypeDouble:
                bindRet = duckdb_bind_double(stmt, i + 1, *(double *)parameters[i]);
                break;
            case Sqlite3TypeText:
                bindRet = duckdb_bind_varchar(stmt, i + 1, parameters[i]);
                break;
            case Sqlite3TypeBlob:
                bindRet = duckdb_bind_blob(stmt, i + 1, parameters[i], length[i]);
                break;
            case Sqlite3TypeNull:
                bindRet = duckdb_bind_null(stmt, i + 1);
                break;
            default:
                LOG_FATAL << "DuckDB does not recognize the parameter type";
                abort();
        }

        if (bindRet != DuckDBSuccess)
        {
            onError(sql, exceptCallback, "Failed to bind parameter");
            idleCb_();
            return;
        }
    }

    // 执行语句
    auto resultPtr = stmtExecute(stmt);

    if (!resultPtr)
    {
        onError(sql, exceptCallback, "Failed to execute statement");
        idleCb_();
        return;
    }

    // 缓存预编译语句
    if (paraNum > 0 && newStmt)
    {
        auto r = stmts_.insert(std::string{sql});
        stmtsMap_[std::string_view{r.first->data(), r.first->length()}] =
            stmtPtr;
    }

    rcb(Result(std::move(resultPtr)));
    idleCb_();
}

std::shared_ptr<DuckdbResultImpl> DuckdbConnection::stmtExecute(
    duckdb_prepared_statement stmt)
{
    // 在堆上分配 duckdb_result
    auto rawResult = new duckdb_result();
    auto state = duckdb_execute_prepared(stmt, rawResult);

    if (state == DuckDBError)
    {
        duckdb_destroy_result(rawResult);
        delete rawResult;
        return nullptr;
    }

    // 创建智能指针，自定义删除器调用 duckdb_destroy_result
    auto resultShared = std::shared_ptr<duckdb_result>(
        rawResult,
        [](duckdb_result *ptr) {
            if (ptr)
            {
                duckdb_destroy_result(ptr);
                delete ptr;
            }
        });

    // 获取受影响的行数
    idx_t rowCount = duckdb_row_count(rawResult);

    // DuckDB C API 没有直接的 last_insert_id
    // 需要通过 RETURNING 子句或其他方式获取
    unsigned long long insertId = 0;

    // 使用带参数的构造函数创建 DuckdbResultImpl
    return std::make_shared<DuckdbResultImpl>(resultShared, rowCount, insertId);
}

void DuckdbConnection::disconnect()
{
    std::promise<int> pro;
    auto f = pro.get_future();
    auto thisPtr = shared_from_this();
    std::weak_ptr<DuckdbConnection> weakPtr = thisPtr;
    loopThread_.getLoop()->runInLoop([weakPtr, &pro]() {
        {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            thisPtr->stmtsMap_.clear();
            thisPtr->stmts_.clear();
            thisPtr->connectionPtr_.reset();
            thisPtr->databasePtr_.reset();
        }
        pro.set_value(1);
    });
    f.get();
}
