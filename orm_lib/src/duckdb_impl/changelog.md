# DuckDB 支持更新日志

## 版本信息
- **分支**: duckdb-add-branch
- **日期**: 2025-11-19
- **功能**: 为 Drogon 框架添加 DuckDB 数据库支持

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
};

// 更新 DbConfig variant 类型
using DbConfig = std::variant<PostgresConfig, MysqlConfig, Sqlite3Config, DuckdbConfig>;
2. lib/src/ConfigLoader.cc
修改内容: 允许 DuckDB 使用空的 dbname 修改位置: 第 546 行
// 修改前
if (dbname.empty() && type != "sqlite3")

// 修改后
if (dbname.empty() && type != "sqlite3" && type != "duckdb")
3. lib/src/HttpAppFrameworkImpl.cc
修改内容: 添加 DuckDB 类型处理分支 修改位置: 第 1003-1010 行
```
// 新增 DuckDB 处理分支
else if (dbType == "duckdb")
{
    addDbClient(orm::DuckdbConfig{connectionNum, filename, name, timeout});
}
else
{
    LOG_ERROR << "Unsupported database type: " << dbType
              << ", should be one of (postgresql, mysql, sqlite3, duckdb)";
}
```
4. orm_lib/src/DbClientManager.cc
修改内容: 添加 DuckDB 客户端创建和配置存储逻辑 修改位置 1: 第 144-154 行 (客户端创建)
```
else if (std::holds_alternative<DuckdbConfig>(dbInfo.config_))
{
    auto &cfg = std::get<DuckdbConfig>(dbInfo.config_);
    dbClientsMap_[cfg.name] =
        drogon::orm::DbClient::newDuckDbClient(dbInfo.connectionInfo_,
                                               cfg.connectionNumber);
    if (cfg.timeout > 0.0)
    {
        dbClientsMap_[cfg.name]->setTimeout(cfg.timeout);
    }

}
```
修改位置 2: 第 239-251 行 (配置存储)
```
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
5. orm_lib/src/DbClientImpl.cc
修改内容: 更新 include 路径 修改位置: 头文件引用部分
```
#if USE_DUCKDB
#include "duckdb_impl/DuckdbConnection.h"
#endif
```
6. CMakeLists.txt
修改内容: 更新 DuckDB 源文件路径 修改位置: DROGON_SOURCES 定义部分
```
set(DROGON_SOURCES
    ${DROGON_SOURCES}
    orm_lib/src/duckdb_impl/DuckdbConnection.cc
    orm_lib/src/duckdb_impl/DuckdbResultImpl.cc)
```
## 使用示例
### JSON 配置文件
```
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
```
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