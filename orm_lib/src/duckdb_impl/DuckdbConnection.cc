/**
 *
 *  @file DuckdbConnection.cc
 *  @author Dq Wei
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
    // 创建 SqlError 异常并通过回调传递
    auto exceptPtr = std::make_exception_ptr(
        SqlError(errorMessage ? errorMessage : "Unknown DuckDB error",
                 std::string{sql}));
    exceptCallback(exceptPtr);
}

/**
 * @brief Construct a new Duckdb Connection:: Duckdb Connection object
 * 
 * @param loop 
 * @param connInfo 连接字符串
 * @param sharedMutex 
 * @param configOptions 配置参数
 */
DuckdbConnection::DuckdbConnection(
    trantor::EventLoop *loop,
    const std::string &connInfo,
    const std::shared_ptr<SharedMutex> &sharedMutex,
    const std::unordered_map<std::string, std::string> &configOptions)  // 新增配置参数支持 [dq 2025-11-21]
    : DbConnection(loop),
      sharedMutexPtr_(sharedMutex),
      connInfo_(connInfo),
      configOptions_(configOptions)  // 存储配置选项 [dq 2025-11-21]
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

    // 在事件循环线程中初始化 DuckDB 数据库连接
    loop_->runInLoop([this, filename = std::move(filename)]() {
        duckdb_database db;
        duckdb_config config;
        duckdb_connection conn;

        // create the configuration object
        if (duckdb_create_config(&config) == DuckDBError) {
            LOG_FATAL << "Failed to config DuckDB database: " << filename;
            auto thisPtr = shared_from_this();
            closeCallback_(thisPtr);
            return;
        }

        // 应用配置选项（使用传入的配置，或使用默认值）[dq 2025-11-19]
        // 定义默认值
        std::unordered_map<std::string, std::string> defaultConfig = {
            {"access_mode", "READ_WRITE"},
            {"threads", "4"},  // 默认改为4以避免过度占用资源
            {"max_memory", "4GB"},
        };

        // 合并用户配置和默认配置（用户配置优先）
        for (const auto& [key, defaultValue] : defaultConfig) {
            auto it = configOptions_.find(key);
            const std::string& value = (it != configOptions_.end()) ? it->second : defaultValue;

            if (duckdb_set_config(config, key.c_str(), value.c_str()) == DuckDBError) {
                LOG_WARN << "Failed to set DuckDB config option: " << key << "=" << value;
            }
        }

        // 应用其他用户自定义的配置选项
        for (const auto& [key, value] : configOptions_) {
            if (defaultConfig.find(key) == defaultConfig.end()) {
                if (duckdb_set_config(config, key.c_str(), value.c_str()) == DuckDBError) {
                    LOG_WARN << "Failed to set DuckDB config option: " << key << "=" << value;
                }
            }
        }

        // 打开数据库
        //auto state = duckdb_open(filename.c_str(), &db);
        auto state = duckdb_open_ext(filename.c_str(), &db, config, NULL);
        if (state == DuckDBError)
        {
            LOG_FATAL << "Failed to open DuckDB database: " << filename;
            duckdb_destroy_config(&config);
            auto thisPtr = shared_from_this();
            closeCallback_(thisPtr);
            return;
        }
        duckdb_destroy_config(&config);

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

/**
 * @brief 执行 SQL 语句（异步）
 * 
 * @param sql SQL 语句 
 * @param paraNum 参数数量
 * @param parameters 参数值
 * @param length 
 * @param format 
 * @param rcb 
 * @param exceptCallback 
 */
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

/**
 * @brief 
 * 
 * @param sql 
 * @param paraNum 
 * @param parameters 
 * @param length 
 * @param format 
 * @param rcb 
 * @param exceptCallback 
 */
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

    // 创建智能指针，删除器调用 duckdb_destroy_result must call！
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



/**
 * @brief 批量执行 SQL 语句（入口方法）
 *
 * @param sqlCommands SQL 命令队列
 */
void DuckdbConnection::batchSql(std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands)
{
    auto thisPtr = shared_from_this();
    loopThread_.getLoop()->queueInLoop(
        [thisPtr, sqlCommands = std::move(sqlCommands)]() mutable {
            thisPtr->batchSqlCommands_ = std::move(sqlCommands);
            thisPtr->executeBatchSql();
        });
}

/**
 * @brief 执行批量 SQL 的主逻辑
 */
void DuckdbConnection::executeBatchSql()
{
    loop_->assertInLoopThread();

    if (batchSqlCommands_.empty())
    {
        idleCb_();
        return;
    }

    // 标记为工作中
    isWorking_ = true;

    // 逐个处理每个命令
    while (!batchSqlCommands_.empty())
    {
        auto cmd = batchSqlCommands_.front();
        executeSingleBatchCommand(cmd);
        batchSqlCommands_.pop_front();
    }

    // 完成所有批处理
    isWorking_ = false;
    idleCb_();
}

/**
 * @brief 执行单个批量 SQL 命令
 *
 * @param cmd SQL 命令对象
 */
void DuckdbConnection::executeSingleBatchCommand(
    const std::shared_ptr<SqlCmd> &cmd)
{
    LOG_TRACE << "DuckDB batch sql:" << cmd->sql_;

    // case1：无参数的 SQL，使用 duckdb_extract_statements
    if (cmd->parametersNumber_ == 0)
    {
        duckdb_extracted_statements extracted;
        idx_t count = duckdb_extract_statements(
            *connectionPtr_,
            cmd->sql_.data(),
            &extracted);

        if (count == 0)
        {
            // 提取失败
            const char *error = duckdb_extract_statements_error(extracted);
            onError(cmd->sql_, cmd->exceptionCallback_, error);
            duckdb_destroy_extracted(&extracted);
            return;
        }

        // 执行提取的语句
        for (idx_t i = 0; i < count; ++i)
        {
            duckdb_prepared_statement stmt;
            if (duckdb_prepare_extracted_statement(
                    *connectionPtr_, extracted, i, &stmt) == DuckDBError)
            {
                const char *error = duckdb_prepare_error(stmt);
                onError(cmd->sql_, cmd->exceptionCallback_, error);
                duckdb_destroy_prepare(&stmt);
                continue;
            }

            // 执行语句
            auto rawResult = new duckdb_result();
            auto state = duckdb_execute_prepared(stmt, rawResult);

            if (state == DuckDBError)
            {
                const char *error = duckdb_result_error(rawResult);
                onError(cmd->sql_, cmd->exceptionCallback_,
                       error ? error : "Unknown execution error");
                duckdb_destroy_result(rawResult);
                delete rawResult;
                duckdb_destroy_prepare(&stmt);
                continue;
            }

            // 成功，创建结果并调用回调
            auto resultShared = std::shared_ptr<duckdb_result>(
                rawResult,
                [](duckdb_result *ptr) {
                    if (ptr)
                    {
                        duckdb_destroy_result(ptr);
                        delete ptr;
                    }
                });

            idx_t rowCount = duckdb_row_count(rawResult);
            auto result = std::make_shared<DuckdbResultImpl>(
                resultShared, rowCount, 0);
            cmd->callback_(Result(result));

            duckdb_destroy_prepare(&stmt);
        }

        duckdb_destroy_extracted(&extracted);
    }
    else
    {
        // case 2：有参数的 SQL，使用现有的预编译逻辑
        execSqlInQueue(cmd->sql_,
                      cmd->parametersNumber_,
                      cmd->parameters_,
                      cmd->lengths_,
                      cmd->formats_,
                      cmd->callback_,
                      cmd->exceptionCallback_);
    }
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
