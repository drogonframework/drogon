# DuckDB 支持更新日志

## 版本信息
- **分支**: duckdb-add-branch
- **日期**: 2025-11-19
- **功能**: 为 Drogon 框架添加 DuckDB 数据库支持

---
## 最新更新 (2025-11-22)
### 功能增强： 实现了 DuckDB 的 batchSql 功能
**修改的文件**
1. DuckdbConnection.h
- 声明了 batchSql() 公开方法
- 添加了两个私有辅助方法：executeBatchSql() 和 executeSingleBatchCommand()
- 添加了批量命令队列成员变量 batchSqlCommands_
2. DuckdbConnection.cc
- 实现了 batchSql() 入口方法（异步入队）
- 实现了 executeBatchSql() 主逻辑（批量循环处理）
- 实现了 executeSingleBatchCommand() 核心方法（使用 duckdb_extract_statements）
**核心特性**
- 使用官方 API：采用 duckdb_extract_statements 进行批量语句解析和执行
- 双路径执行：
  无参数 SQL：使用 duckdb_extract_statements → 支持多语句字符串
  有参数 SQL：使用现有的 execSqlInQueue → 支持参数绑定
- 完善的资源管理：所有 DuckDB 资源都正确清理（extract、prepare、result）
- 独立的错误处理：每个命令独立处理错误，不影响后续命令
- 线程安全：在事件循环线程中执行，使用 queueInLoop 确保线程安全

## 最新更新 (2025-11-19)

### 功能增强：DuckDB 配置参数灵活化支持

**更新说明**：参考 PostgreSQL 的 `connectOptions` 模式，为 DuckDB 添加了灵活的配置参数支持，允许用户通过配置文件自定义 DuckDB 的各种运行参数。

#### 主要改进

1. **配置参数支持**
   - 使用 `std::unordered_map<std::string, std::string>` 存储可变配置参数
   - 支持任意 DuckDB 官方文档中的配置选项
   - 提供合理的默认值（access_mode: READ_WRITE, threads: 4, max_memory: 4GB）
   - 用户配置优先级高于默认值

2. **配置方式**
   - JSON/YAML 配置文件：通过 `config_options` 字段配置
   - 编程式配置：通过 `DuckdbConfig` 结构体或工厂方法
   - 向后兼容：现有代码无需修改

#### 新增/修改文件

| 文件路径 | 修改内容 | 说明 |
|---------|---------|------|
| `orm_lib/inc/drogon/orm/DbConfig.h` | 添加 `configOptions` 字段 | `DuckdbConfig` 结构新增配置参数映射 |
| `orm_lib/src/duckdb_impl/DuckdbConnection.h` | 更新构造函数，添加成员变量 | 支持配置参数传递和存储 |
| `orm_lib/src/duckdb_impl/DuckdbConnection.cc` | 重构 `init()` 方法 | 使用灵活配置替代硬编码，支持默认值机制 |
| `orm_lib/src/DbClientImpl.h` | 新增配置构造函数 | 支持接收配置选项的构造函数 |
| `orm_lib/src/DbClientImpl.cc` | 实现配置构造函数，更新 `newConnection()` | 将配置选项传递给 DuckdbConnection |
| `orm_lib/inc/drogon/orm/DbClient.h` | 新增工厂方法重载 | `newDuckDbClient()` 支持配置参数 |
| `orm_lib/src/DbClient.cc` | 实现新工厂方法 | 创建带配置选项的 DuckDB 客户端 |
| `orm_lib/src/DbClientManager.cc` | 更新客户端创建逻辑 | 传递配置选项到工厂方法 |
| `lib/src/HttpAppFrameworkImpl.cc` | 更新 `addDbClient()` 方法 | 构造 DuckdbConfig 时传递 options |
| `lib/src/ConfigLoader.cc` | 新增 `config_options` 解析 | 从 JSON 配置中提取 DuckDB 配置参数 |

#### 配置示例

**JSON 配置文件**:
```json
{
  "db_clients": [
    {
      "name": "analytics",
      "rdbms": "duckdb",
      "filename": "analytics.duckdb",
      "connection_number": 2,
      "timeout": -1.0,
      "config_options": {
        "access_mode": "READ_WRITE",
        "threads": "8",
        "max_memory": "8GB",
        "default_order": "ASC",
        "enable_external_access": "false",
        "enable_object_cache": "true",
        "checkpoint_threshold": "16MB"
      }
    }
  ]
}
```

**编程式配置**:
```cpp
// 方式1：使用 DuckdbConfig 结构
app().addDbClient(orm::DuckdbConfig{
    2,                    // connectionNumber
    "test.duckdb",       // filename
    "default",           // name
    -1.0,                // timeout
    {
        {"threads", "8"},
        {"max_memory", "8GB"},
        {"access_mode", "READ_WRITE"}
    }  // configOptions
});

// 方式2：使用工厂方法
auto client = DbClient::newDuckDbClient(
    "filename=test.duckdb",
    2,
    {
        {"threads", "4"},
        {"max_memory", "4GB"}
    }
);
```

#### 支持的配置参数

常用配置参数（详见 [DuckDB 官方文档](https://duckdb.org/docs/stable/configuration/overview)）：

| 参数名 | 说明 | 示例值 |
|--------|------|--------|
| `access_mode` | 访问模式 | READ_ONLY, READ_WRITE |
| `threads` | 线程数 | "4", "8", "16" |
| `max_memory` | 最大内存 | "4GB", "8GB", "16GB" |
| `default_order` | 默认排序 | ASC, DESC |
| `enable_external_access` | 外部访问 | "true", "false" |
| `enable_object_cache` | 对象缓存 | "true", "false" |
| `checkpoint_threshold` | 检查点阈值 | "16MB", "32MB", "64MB" |

#### 默认配置值

未配置时的默认值：
- `access_mode`: `"READ_WRITE"`
- `threads`: `"4"`
- `max_memory`: `"4GB"`

#### 技术实现细节

1. **配置流程**:
   ```
   JSON配置 → ConfigLoader → HttpAppFrameworkImpl → DbConfig →
   DbClientManager → DbClient → DbClientImpl → DuckdbConnection
   ```

2. **配置优先级**:
   - 用户自定义配置 > 默认配置
   - 用户可覆盖任意默认值
   - 用户可添加任意 DuckDB 支持的参数

3. **向后兼容性**:
   - 现有代码无需修改
   - 不提供 `config_options` 时使用默认值
   - 新旧 API 可以共存

#### 代码标记

所有新增/修改的代码均使用 `[dq 2025-11-19]` 注释标记，便于识别和维护。

---

## 新增文件

| 文件路径 | 说明 |
|---------|------|
| `orm_lib/src/duckdb_impl/DuckdbConnection.h` | DuckDB 连接类头文件，管理数据库连接、预编译语句缓存 |
| `orm_lib/src/duckdb_impl/DuckdbConnection.cc` | DuckDB 连接实现，包含 SQL 执行、参数绑定、事务支持 |
| `orm_lib/src/duckdb_impl/DuckdbResultImpl.h` | DuckDB 结果集头文件，使用智能指针管理 `duckdb_result` |
| `orm_lib/src/duckdb_impl/DuckdbResultImpl.cc` | DuckDB 结果集实现，按需类型转换，值缓存机制 |
| `orm_lib/src/duckdb_impl/test/test1.cc` | 综合测试套件，包含 20 个测试类别 |

---

## 修改文件

### 1. orm_lib/inc/drogon/orm/DbConfig.h

**修改内容**: 添加 DuckDB 配置结构体

**修改位置**: 第 61-69 行

```cpp
// 新增 DuckdbConfig 结构体
struct DuckdbConfig
{
    size_t connectionNumber;
    std::string filename;
    std::string name;
    double timeout;
    std::unordered_map<std::string, std::string> configOptions;  // DuckDB配置选项
};

// 更新 DbConfig variant 类型
using DbConfig = std::variant<PostgresConfig, MysqlConfig, Sqlite3Config, DuckdbConfig>;
```

### 2. lib/src/ConfigLoader.cc

**修改内容**: 允许 DuckDB 使用空的 dbname，添加 config_options 解析

**修改位置**: 第 546 行、第 575-594 行

```cpp
// 修改前
if (dbname.empty() && type != "sqlite3")

// 修改后
if (dbname.empty() && type != "sqlite3" && type != "duckdb")

// 新增 DuckDB config_options 解析
if (type == "duckdb")
{
    auto configOptions = client.get("config_options", Json::Value());
    if (configOptions.isObject() && !configOptions.empty())
    {
        for (const auto &key : configOptions.getMemberNames())
        {
            options[key] = configOptions[key].asString();
        }
    }
}
```

### 3. lib/src/HttpAppFrameworkImpl.cc

**修改内容**: 添加 DuckDB 类型处理分支，传递配置选项

**修改位置**: 第 1003-1007 行

```cpp
// 新增 DuckDB 处理分支
else if (dbType == "duckdb")
{
    // 传递配置选项到DuckdbConfig
    addDbClient(orm::DuckdbConfig{connectionNum, filename, name, timeout, std::move(options)});
}
else
{
    LOG_ERROR << "Unsupported database type: " << dbType
              << ", should be one of (postgresql, mysql, sqlite3, duckdb)";
}
```

### 4. orm_lib/src/DbClientManager.cc

**修改内容**: 添加 DuckDB 客户端创建和配置存储逻辑，传递配置选项

**修改位置 1**: 第 144-156 行 (客户端创建)

```cpp
else if (std::holds_alternative<DuckdbConfig>(dbInfo.config_))
{
    auto &cfg = std::get<DuckdbConfig>(dbInfo.config_);
    // 使用带配置选项的工厂方法
    dbClientsMap_[cfg.name] =
        drogon::orm::DbClient::newDuckDbClient(dbInfo.connectionInfo_,
                                               cfg.connectionNumber,
                                               cfg.configOptions);  // 传递配置选项
    if (cfg.timeout > 0.0)
    {
        dbClientsMap_[cfg.name]->setTimeout(cfg.timeout);
    }
}
```

**修改位置 2**: 第 250-262 行 (配置存储)

```cpp
else if (std::holds_alternative<DuckdbConfig>(config))
{
#if USE_DUCKDB
    auto cfg = std::get<DuckdbConfig>(config);
    std::string connStr = "filename=" + cfg.filename;
    dbInfos_.emplace_back(DbInfo{connStr, config});
#else
    std::cout << "The DuckDB is not supported in current drogon build, "
                 "please install the development library first."
              << std::endl;
    exit(1);
#endif
}
```

### 5. orm_lib/src/DbClientImpl.h

**修改内容**: 添加 DuckDB 配置构造函数声明和成员变量

**修改位置**: 第 43-47 行、第 77 行

```cpp
// DuckDB配置构造函数
DbClientImpl(const std::string &connInfo,
             size_t connNum,
             ClientType type,
             const std::unordered_map<std::string, std::string> &configOptions);

// 成员变量
std::unordered_map<std::string, std::string> configOptions_;  // DuckDB配置选项
```

### 6. orm_lib/src/DbClientImpl.cc

**修改内容**: 实现 DuckDB 配置构造函数，更新 newConnection() 传递配置

**修改位置 1**: 第 78-99 行 (构造函数实现)

```cpp
// DuckDB配置构造函数实现
DbClientImpl::DbClientImpl(const std::string &connInfo,
                           size_t connNum,
                           ClientType type,
                           const std::unordered_map<std::string, std::string> &configOptions)
    : numberOfConnections_(connNum),
#if LIBPQ_SUPPORTS_BATCH_MODE
      autoBatch_(false),
#endif
      configOptions_(configOptions),  // 存储配置选项
      loops_(...)
{
    type_ = type;
    connectionInfo_ = connInfo;
    LOG_TRACE << "type=" << (int)type;
    assert(connNum > 0);
}
```

**修改位置 2**: 第 435-438 行 (newConnection 更新)

```cpp
connPtr = std::make_shared<DuckdbConnection>(loop,
                                             connectionInfo_,
                                             sharedMutexPtr_,
                                             configOptions_);  // 传递配置选项
```

### 7. orm_lib/inc/drogon/orm/DbClient.h

**修改内容**: 添加带配置选项的 DuckDB 工厂方法声明

**修改位置**: 第 135-139 行

```cpp
// DuckDB with config options support
static std::shared_ptr<DbClient> newDuckDbClient(
    const std::string &connInfo,
    size_t connNum,
    const std::unordered_map<std::string, std::string> &configOptions);
```

### 8. orm_lib/src/DbClient.cc

**修改内容**: 实现带配置选项的 DuckDB 工厂方法

**修改位置**: 第 123-143 行

```cpp
// DuckDB with config options support
std::shared_ptr<DbClient> DbClient::newDuckDbClient(
    const std::string &connInfo,
    size_t connNum,
    const std::unordered_map<std::string, std::string> &configOptions)
{
#if USE_DUCKDB
    auto client = std::make_shared<DbClientImpl>(connInfo,
                                                 connNum,
                                                 ClientType::DuckDB,
                                                 configOptions);
    client->init();
    return client;
#else
    LOG_FATAL << "DuckDB is not supported!";
    exit(1);
#endif
}
```

### 9. orm_lib/src/duckdb_impl/DuckdbConnection.h

**修改内容**: 更新构造函数签名，添加配置选项成员变量

**修改位置**: 第 43-46 行、第 101 行

```cpp
// 构造函数
DuckdbConnection(trantor::EventLoop *loop,
                 const std::string &connInfo,
                 const std::shared_ptr<SharedMutex> &sharedMutex,
                 const std::unordered_map<std::string, std::string> &configOptions = {});

// 成员变量
std::unordered_map<std::string, std::string> configOptions_;  // DuckDB配置选项
```

### 10. orm_lib/src/duckdb_impl/DuckdbConnection.cc

**修改内容**: 实现新构造函数，重构 init() 方法使用灵活配置

**修改位置 1**: 第 44-54 行 (构造函数)

```cpp
DuckdbConnection::DuckdbConnection(
    trantor::EventLoop *loop,
    const std::string &connInfo,
    const std::shared_ptr<SharedMutex> &sharedMutex,
    const std::unordered_map<std::string, std::string> &configOptions)
    : DbConnection(loop),
      sharedMutexPtr_(sharedMutex),
      connInfo_(connInfo),
      configOptions_(configOptions)  // 存储配置选项
{
}
```

**修改位置 2**: 第 92-117 行 (init() 方法配置部分)

```cpp
// 应用配置选项（使用传入的配置，或使用默认值）
// 定义默认值
std::unordered_map<std::string, std::string> defaultConfig = {
    {"access_mode", "READ_WRITE"},
    {"threads", "4"},
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
```

---
## 使用示例
### JSON 配置文件
```json
{
  "db_clients": [
    {
      "name": "default",
      "rdbms": "duckdb",
      "filename": "mydb.duckdb",
      "connection_number": 1,
      "timeout": 10.0
    }
  ]
}
```
### 代码调用
```cpp
// 创建 DuckDB 客户端
auto client = DbClient::newDuckDbClient("filename=mydb.duckdb", 1);

// 执行查询
*client << "SELECT * FROM users WHERE id = ?" << 1 >>
    [](const Result &r) {
        // 处理结果
    } >>
    [](const DrogonDbException &e) {
        // 处理错误
    };
```
### 测试覆盖
测试套件包含 20 个测试类别：
基本数据类型
NULL 值处理
CRUD 操作
事务支持
JOIN 查询
聚合函数
子查询
视图
索引
约束
预编译语句
异步操作
错误处理
复杂查询
日期时间函数
字符串函数
多连接
execSqlSync
execSqlAsyncFuture
批量操作
依赖要求
DuckDB C API 库
CMake 选项: -DBUILD_DUCKDB=ON
兼容性说明
支持 DuckDB 所有标准 SQL 功能
参数绑定使用 ? 占位符
强类型数据库，自动进行类型转换