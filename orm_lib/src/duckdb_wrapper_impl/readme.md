#采用DcukDb的sqltie兼容接口的实现
# 于 2025-7-3 添加于Jeddah
嵌入DuckDB的[sqlite3_api_wrapper](https://github.com/weidqi/duckdb/tree/main/tools/sqlite3_api_wrapper)的官方实现代码,然后像连接SQLite一样连接DuckDB.这适用于SQLITE的既有用户,但无法获得DUCKDB的全部特性.

## 1 嵌入代码
由于Drogon同时使用了sqlite3,因此采用调用外部动态库的十分容易造成混淆的头文件`sqlite3.h`,所以创建一个新的头文件`sqlite3_duck.h`
1. 修改主cmakelist,引入DuckDB标准库
``
```
# Add DuckDB support
if (BUILD_DUCKDB)
    # Find DuckDB library
    find_package(DuckDB QUIET)
    if(NOT DuckDB_FOUND)
        # Try to find DuckDB manually
        find_library(DUCKDB_LIBRARY
            NAMES duckdb
            PATHS /usr/lib /usr/local/lib
        )
        find_path(DUCKDB_INCLUDE_DIR
            NAMES duckdb.h
            PATHS /usr/include /usr/local/include 
        )
        
        if(DUCKDB_LIBRARY AND DUCKDB_INCLUDE_DIR)
            set(DuckDB_LIBRARIES ${DUCKDB_LIBRARY})
            set(DuckDB_INCLUDE_DIRS ${DUCKDB_INCLUDE_DIR})
            set(DuckDB_FOUND TRUE)
        endif()
    endif()

    if (DuckDB_FOUND)
        message(STATUS "Ok! We find DuckDB!")
        target_link_libraries(${PROJECT_NAME} PRIVATE ${DuckDB_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${DuckDB_INCLUDE_DIRS})
        set(DROGON_FOUND_DUCKDB TRUE)
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            orm_lib/src/duckdb_impl/DuckDBConnection.cc
            orm_lib/src/duckdb_impl/DuckDBResultImpl.cc
            orm_lib/src/duckdb_wrapper_impl/DuckDBConnection.cc
            orm_lib/src/duckdb_wrapper_impl/DuckDBResultImpl.cc)
        set(private_headers
            ${private_headers}
            orm_lib/src/duckdb_impl/DuckDBConnection.h
            orm_lib/src/duckdb_impl/DuckDBResultImpl.h
            )
    else (DuckDB_FOUND)
        message(STATUS "DuckDB was not found.")
        set(DROGON_FOUND_DUCKDB FALSE)
    endif (DuckDB_FOUND)
else (BUILD_DUCKDB)
    set(DROGON_FOUND_DUCKDB FALSE)
endif (BUILD_DUCKDB)

```
2. 该实现依赖第三方库`utf8proc`,需引入

## 2 实现接口
由于做了接口适配器,所以实现照搬sqlite3的实现即可.
## 3 修改框架代码
1. 
2. SqlBinder.h, SqlBinder.cc的改动
首先ClientTpye增加一个值:
```
enum class ClientType
{
    ...
    DuckDB
};
```
其次,在所有:
`if (type_ == ClientType::Sqlite3)`的地方改为:
`if (type_ == ClientType::Sqlite3 || type_ == ClientType::DuckDB)`
涉及到改动的文件还有Mapper.h的一处地方