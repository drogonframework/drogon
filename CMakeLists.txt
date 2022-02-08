cmake_minimum_required(VERSION 3.5)
# ##############################################################################
# Project Name and Version
# ##############################################################################
project(drogon)

# version
set(DROGON_MAJOR_VERSION 1)
set(DROGON_MINOR_VERSION 7)
set(DROGON_PATCH_VERSION 4)
set(DROGON_VERSION ${DROGON_MAJOR_VERSION}.${DROGON_MINOR_VERSION}.${DROGON_PATCH_VERSION})
set(DROGON_VERSION_STRING "${DROGON_VERSION}")
# git sha1
execute_process(COMMAND "git" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_SHA1
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/version.h.in"
    "${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h" @ONLY)

# ##############################################################################
# Complier and C++ standard checking
# ##############################################################################
# Complier
message(STATUS "Compiler: " ${CMAKE_CXX_COMPILER_ID} 
               " " ${CMAKE_CXX_COMPILER_VERSION}
               " " ${CMAKE_PLATFORM_NAME}
               " " ${CMAKE_SYSTEM_NAME})

set(COMPILER_COMMAND ${CMAKE_CXX_COMPILER})
set(COMPILER_ID ${CMAKE_CXX_COMPILER_ID})

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Force MSVC to use UTF-8 because that's what we use. Otherwise it uses
    # the default of whatever Windows sets and causes encoding issues.
    message(STATUS "You are using MSVC. Forceing to use UTF-8")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

    # warning level 4 and all warnings as errors
    # add_compile_options(/W4 /WX)
endif ()
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
   # lots of warnings and all warnings as errors
   # add_compile_options(-Wall -Wextra -Werror)
endif ()

# Cxx_standard checking 
include(CheckIncludeFileCXX)
check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
message(STATUS "HAS_ANY:" ${HAS_ANY})
message(STATUS "HAS_STRING_VIEW:" ${HAS_STRING_VIEW})
message(STATUS "HAS_COROUTINE:" ${HAS_COROUTINE})

if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    set(DROGON_CXX_STANDARD ${CMAKE_CXX_STANDARD})
    message(STATUS "use" ${CMAKE_CXX_STANDARD})
elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(DROGON_CXX_STANDARD 20)
    message(STATUS "use c++20")
elseif (HAS_ANY AND HAS_STRING_VIEW)
    set(DROGON_CXX_STANDARD 17)    
    message(STATUS "use c++17")
else ()
    set(DROGON_CXX_STANDARD 14)
    message(STATUS "use c++14")
endif ()

if (DROGON_CXX_STANDARD EQUAL 20)
    option(USE_COROUTINE "Enable C++20 coroutine support" ON)
else ()
    option(USE_COROUTINE "Enable C++20 coroutine support" OFF)
endif ()

# ##############################################################################
# Build type
# ##############################################################################
if (CMAKE_BUILD_TYPE)
    string(TOLOWER ${CMAKE_BUILD_TYPE} _type)
    if (_type STREQUAL release)
        set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -std=c++")
    elseif (_type STREQUAL debug)
        set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++")
    else ()
        set(COMPILATION_FLAGS "-std=c++")
    endif ()
else ()
    set(COMPILATION_FLAGS "-std=c++")
endif ()

# ##############################################################################
# If your cross compile is failing, you should set
# CMAKE_SYSTEM_NAME in your toolchain file
if (NOT CMAKE_CROSSCOMPILING)
    set(BUILD_PROGRAMS ON)
else ()
    set(BUILD_PROGRAMS OFF)
endif ()

# Build Options
option(BUILD_CTL "Build drogon_ctl" ${BUILD_PROGRAMS})
option(BUILD_EXAMPLES "Build examples" ${BUILD_PROGRAMS})
option(BUILD_ORM "Build orm" ON)
option(COZ_PROFILING "Use coz for profiling" OFF)
option(BUILD_DROGON_SHARED "Build drogon as a shared lib" OFF)
option(BUILD_DOC "Build Doxygen documentation" OFF)
option(BUILD_BROTLI "Build Brotli" ON)

include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_POSTGRESQL "Build with postgresql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(LIBPQ_BATCH_MODE "Use batch mode for libpq" ON "BUILD_POSTGRESQL" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_MYSQL "Build with mysql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_SQLITE "Build with sqlite3 support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_REDIS "Build with redis support" ON "BUILD_ORM" OFF)

# Build shared or static
if (BUILD_DROGON_SHARED)
    set(BUILD_TRANTOR_SHARED TRUE)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
    find_package(Threads)
    # set(BUILD_EXAMPLES FALSE)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
        "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" isSystemDir)
    if ("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")
    endif ()

    add_library(${PROJECT_NAME} SHARED)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${DROGON_VERSION}
        SOVERSION ${DROGON_MAJOR_VERSION})
    target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PUBLIC Rpcrt4 ws2_32 crypt32 Advapi32)
        if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            # Ignore MSVC C4251 and C4275 warning of exporting std objects with no dll export
            # We export class to facilitate maintenance, thus if you compile
            # drogon on windows as a shared library, you will need to use
            # exact same compiler for drogon and your app.
            target_compile_options(${PROJECT_NAME} PUBLIC /wd4251 /wd4275)
        endif ()
    endif ()
else (BUILD_DROGON_SHARED)
    add_library(${PROJECT_NAME} STATIC)
endif (BUILD_DROGON_SHARED)

# Target C++ standard
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD ${DROGON_CXX_STANDARD})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

# Platform
if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    target_link_libraries(${PROJECT_NAME} PRIVATE network)
elseif (NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
elseif (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)
endif ()

# ##############################################################################
# Add cmake_modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

###############################################################################
# filesystem (std::filesystem or boost::filesystem)
# ##############################################################################
set(NEED_BOOST_FS 0)
find_package(Filesystem COMPONENTS Final)

if(CXX_FILESYSTEM_HAVE_FS)
    message(STATUS "Found std::filesystem")
    option(HAS_STD_FILESYSTEM_PATH "use std::filesystem" ON)
    if (DROGON_CXX_STANDARD EQUAL 17)
        # Check for partial implementation of c++17 (Windows/OSX only?)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        try_compile(check_filesystem_path ${CMAKE_BINARY_DIR}/cmaketest
            ${PROJECT_SOURCE_DIR}/cmake/tests/check_has_std_filesystem_path.cc
            CXX_STANDARD 17)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE)
        if (NOT check_filesystem_path)
          message(STATUS "The std::filesystem seems to be a partial implementation"
            " Falling back to boost::filesystem")
            set(NEED_BOOST_FS 1)
        endif()
    else ()
    endif()

    # HACK: Needed to be compiled on Yocto Linux
    get_property(CAN_LINK_FS TARGET std::filesystem PROPERTY INTERFACE_LINK_LIBRARIES SET)
    if (CAN_LINK_FS)
    target_link_libraries(${PROJECT_NAME} PUBLIC std::filesystem)
    endif()
else()
   option(HAS_STD_FILESYSTEM_PATH "use std::filesystem" OFF)
   set(NEED_BOOST_FS 1)
endif()

# Workaround: 2021-08-09 Android NDK does not provide proper std::filesystem
# support. Force boost::filesystem instead.
if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    message(STATUS "WORKAROUND: Forcing boost::filesystem on Android")
    set(NEED_BOOST_FS 1)
endif ()

if(NEED_BOOST_FS)
    find_package(Boost 1.61.0 COMPONENTS filesystem system REQUIRED)
    message(STATUS "Using Boost filesytem::path")
    message(STATUS "Boost include dir: " ${Boost_INCLUDE_DIR})
    include_directories(${BOOST_INCLUDE_DIRS})
    message(STATUS "Boost libraries: " ${Boost_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::filesystem Boost::system)
    list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${Boost_INCLUDE_DIR})
endif()

# ##############################################################################
# essential dependencies
# ##############################################################################
# submodule
add_subdirectory(trantor)
target_link_libraries(${PROJECT_NAME} PUBLIC trantor)

# jsoncpp
find_package(Jsoncpp REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Jsoncpp_lib)
list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${JSONCPP_INCLUDE_DIRS})

# uuid
if (NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" AND NOT CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" AND NOT WIN32)
    find_package(UUID REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE UUID_lib)

    try_compile(normal_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/normal_uuid_lib_test.cc
        LINK_LIBRARIES UUID_lib)
    try_compile(ossp_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/ossp_uuid_lib_test.cc
        LINK_LIBRARIES UUID_lib)
    if (normal_uuid)
        add_definitions(-DUSE_OSSP_UUID=0)
    elseif (ossp_uuid)
        add_definitions(-DUSE_OSSP_UUID=1)
    else ()
        message(FATAL_ERROR "uuid lib error")
    endif ()
endif ()

# zlib
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    message(STATUS "Founld zlib")
    target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
endif ()

# ##############################################################################
# headers&sources definitions
# ##############################################################################
set(DROGON_HEADERS
    lib/inc/drogon/Attribute.h
    lib/inc/drogon/CacheMap.h
    lib/inc/drogon/Cookie.h
    lib/inc/drogon/DrClassMap.h
    lib/inc/drogon/DrObject.h
    lib/inc/drogon/DrTemplate.h
    lib/inc/drogon/DrTemplateBase.h
    lib/inc/drogon/HttpAppFramework.h
    lib/inc/drogon/HttpBinder.h
    lib/inc/drogon/HttpClient.h
    lib/inc/drogon/HttpController.h
    lib/inc/drogon/HttpFilter.h
    lib/inc/drogon/HttpRequest.h
    lib/inc/drogon/HttpResponse.h
    lib/inc/drogon/HttpSimpleController.h
    lib/inc/drogon/HttpTypes.h
    lib/inc/drogon/HttpViewData.h
    lib/inc/drogon/IntranetIpFilter.h
    lib/inc/drogon/IOThreadStorage.h
    lib/inc/drogon/LocalHostFilter.h
    lib/inc/drogon/MultiPart.h
    lib/inc/drogon/NotFound.h
    lib/inc/drogon/Session.h
    lib/inc/drogon/UploadFile.h
    lib/inc/drogon/WebSocketClient.h
    lib/inc/drogon/WebSocketConnection.h
    lib/inc/drogon/WebSocketController.h
    lib/inc/drogon/drogon.h
    lib/inc/drogon/version.h
    lib/inc/drogon/drogon_callbacks.h
    lib/inc/drogon/PubSubService.h
    lib/inc/drogon/drogon_test.h
    ${CMAKE_CURRENT_BINARY_DIR}/exports/drogon/exports.h)
set(private_headers
    lib/src/AOPAdvice.h
    lib/src/CacheFile.h
    lib/src/ConfigLoader.h
    lib/src/filesystem.h
    lib/src/FiltersFunction.h
    lib/src/HttpAppFrameworkImpl.h
    lib/src/HttpClientImpl.h
    lib/src/HttpControllersRouter.h
    lib/src/HttpFileImpl.h
    lib/src/HttpFileUploadRequest.h
    lib/src/HttpMessageBody.h
    lib/src/HttpRequestImpl.h
    lib/src/HttpRequestParser.h
    lib/src/HttpResponseImpl.h
    lib/src/HttpResponseParser.h
    lib/src/HttpServer.h
    lib/src/HttpSimpleControllersRouter.h
    lib/src/HttpUtils.h
    lib/src/impl_forwards.h
    lib/src/ListenerManager.h
    lib/src/PluginsManager.h
    lib/src/SessionManager.h
    lib/src/SpinLock.h
    lib/src/StaticFileRouter.h
    lib/src/TaskTimeoutFlag.h
    lib/src/WebSocketClientImpl.h
    lib/src/WebSocketConnectionImpl.h
    lib/src/WebsocketControllersRouter.h
    lib/src/DbClientManager.h
    orm_lib/src/DbClientImpl.h
    orm_lib/src/DbConnection.h
    orm_lib/src/ResultImpl.h
    orm_lib/src/TransactionImpl.h)
set(ORM_HEADERS
    orm_lib/inc/drogon/orm/ArrayParser.h
    orm_lib/inc/drogon/orm/Criteria.h
    orm_lib/inc/drogon/orm/DbClient.h
    orm_lib/inc/drogon/orm/DbTypes.h
    orm_lib/inc/drogon/orm/Exception.h
    orm_lib/inc/drogon/orm/Field.h
    orm_lib/inc/drogon/orm/FunctionTraits.h
    orm_lib/inc/drogon/orm/Mapper.h
    orm_lib/inc/drogon/orm/CoroMapper.h
    orm_lib/inc/drogon/orm/Result.h
    orm_lib/inc/drogon/orm/ResultIterator.h
    orm_lib/inc/drogon/orm/Row.h
    orm_lib/inc/drogon/orm/RowIterator.h
    orm_lib/inc/drogon/orm/SqlBinder.h
    orm_lib/inc/drogon/orm/RestfulController.h)
set(NOSQL_HEADERS
    nosql_lib/redis/inc/drogon/nosql/RedisClient.h
    nosql_lib/redis/inc/drogon/nosql/RedisResult.h
    nosql_lib/redis/inc/drogon/nosql/RedisException.h)
set(DROGON_UTIL_HEADERS
    lib/inc/drogon/utils/FunctionTraits.h
    lib/inc/drogon/utils/Utilities.h
    lib/inc/drogon/utils/any.h
    lib/inc/drogon/utils/string_view.h
    lib/inc/drogon/utils/optional.h
    lib/inc/drogon/utils/coroutine.h
    lib/inc/drogon/utils/HttpConstraint.h
    lib/inc/drogon/utils/OStringStream.h)
set(DROGON_PLUGIN_HEADERS
    lib/inc/drogon/plugins/Plugin.h
    lib/inc/drogon/plugins/SecureSSLRedirector.h
    lib/inc/drogon/plugins/AccessLogger.h)
set(DROGON_SOURCES
    lib/src/AOPAdvice.cc
    lib/src/CacheFile.cc
    lib/src/ConfigLoader.cc
    lib/src/Cookie.cc
    lib/src/DrClassMap.cc
    lib/src/DrTemplateBase.cc
    lib/src/FiltersFunction.cc
    lib/src/HttpAppFrameworkImpl.cc
    lib/src/HttpBinder.cc
    lib/src/HttpClientImpl.cc
    lib/src/HttpControllersRouter.cc
    lib/src/HttpFileImpl.cc
    lib/src/HttpFileUploadRequest.cc
    lib/src/HttpRequestImpl.cc
    lib/src/HttpRequestParser.cc
    lib/src/HttpResponseImpl.cc
    lib/src/HttpResponseParser.cc
    lib/src/HttpServer.cc
    lib/src/HttpSimpleControllersRouter.cc
    lib/src/HttpUtils.cc
    lib/src/HttpViewData.cc
    lib/src/IntranetIpFilter.cc
    lib/src/ListenerManager.cc
    lib/src/LocalHostFilter.cc
    lib/src/MultiPart.cc
    lib/src/NotFound.cc
    lib/src/PluginsManager.cc
    lib/src/RangeParser.cc
    lib/src/SecureSSLRedirector.cc
    lib/src/AccessLogger.cc
    lib/src/SessionManager.cc
    lib/src/StaticFileRouter.cc
    lib/src/TaskTimeoutFlag.cc
    lib/src/Utilities.cc
    lib/src/WebSocketClientImpl.cc
    lib/src/WebSocketConnectionImpl.cc
    lib/src/WebsocketControllersRouter.cc    
    orm_lib/src/ArrayParser.cc
    orm_lib/src/Criteria.cc
    orm_lib/src/DbClient.cc
    orm_lib/src/DbClientImpl.cc
    orm_lib/src/DbClientLockFree.cc
    orm_lib/src/DbConnection.cc
    orm_lib/src/Exception.cc
    orm_lib/src/Field.cc
    orm_lib/src/Result.cc
    orm_lib/src/Row.cc
    orm_lib/src/SqlBinder.cc
    orm_lib/src/TransactionImpl.cc
    orm_lib/src/RestfulController.cc)

if (NOT WIN32)
    set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/SharedLibManager.cc)
    set(private_headers ${private_headers} lib/src/SharedLibManager.h)
else (NOT WIN32)
    set(DROGON_SOURCES ${DROGON_SOURCES} third_party/mman-win32/mman.c)
    set(private_headers ${private_headers} third_party/mman-win32/mman.h)
endif (NOT WIN32)

# dynamic view
list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW
    "${CMAKE_INSTALL_PREFIX}/${INSTALL_INCLUDE_DIR}")
list(REMOVE_DUPLICATES INCLUDE_DIRS_FOR_DYNAMIC_VIEW)
set(INS_STRING "")
foreach (loop_var ${INCLUDE_DIRS_FOR_DYNAMIC_VIEW})
    set(INS_STRING "${INS_STRING} -I${loop_var}")
endforeach (loop_var)
set(INCLUDING_DIRS ${INS_STRING})

# ##############################################################################
# optional dependencies
# ##############################################################################
# Brotli
if (BUILD_BROTLI)
    find_package(Brotli)
    if (Brotli_FOUND)
        add_definitions(-DUSE_BROTLI)
        target_link_libraries(${PROJECT_NAME} PRIVATE Brotli_lib)
    endif (Brotli_FOUND)
endif (BUILD_BROTLI)

# COZ_PROFILING
if (COZ_PROFILING)
    find_package(coz-profiler REQUIRED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOZ_PROFILING=1)
    # If linked will not need to be ran with `coz run --- [executable]` to run the
    # profiler, but drogon_ctl currently won't build because it doesn't find debug
    # information while trying to generate it's own sources
    # target_link_libraries(${PROJECT_NAME} PUBLIC coz::coz)
    target_include_directories(${PROJECT_NAME} PUBLIC ${COZ_INCLUDE_DIRS})
endif (COZ_PROFILING)

# OpenSSL
find_package(OpenSSL)
if (OpenSSL_FOUND)
    message(STATUS "Founld OpenSSL")
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else (OpenSSL_FOUND)
    set(DROGON_SOURCES
        ${DROGON_SOURCES}
        lib/src/ssl_funcs/Md5.cc
        lib/src/ssl_funcs/Sha1.cc)
    set(private_headers
        ${private_headers}
        lib/src/ssl_funcs/Md5.h
        lib/src/ssl_funcs/Sha1.h)
endif (OpenSSL_FOUND)

# POSTGRESQL
if (BUILD_POSTGRESQL)
    # find postgres
    find_package(pg QUIET)
    if (pg_FOUND)
        message(STATUS "Ok! We find Postgresql!")
        option(USE_POSTGRESQL "Enable PostgreSQL" ON)
        message(STATUS "libpq inc path:" ${PG_INCLUDE_DIRS})
        message(STATUS "libpq lib:" ${PG_LIBRARIES})
        target_link_libraries(${PROJECT_NAME} PRIVATE pg_lib)
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc)
        set(private_headers
            ${private_headers}
            orm_lib/src/postgresql_impl/PostgreSQLResultImpl.h)
        if (LIBPQ_BATCH_MODE)
            try_compile(libpq_supports_batch ${CMAKE_BINARY_DIR}/cmaketest
                ${PROJECT_SOURCE_DIR}/cmake/tests/test_libpq_batch_mode.cc
                LINK_LIBRARIES ${PostgreSQL_LIBRARIES}
                CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PostgreSQL_INCLUDE_DIR}")
        endif (LIBPQ_BATCH_MODE)
        if (libpq_supports_batch)
            message(STATUS "The libpq supports batch mode")
            option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" ON)
            set(DROGON_SOURCES
                ${DROGON_SOURCES}
                orm_lib/src/postgresql_impl/PgBatchConnection.cc)
        else (libpq_supports_batch)
            option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" OFF)
            set(DROGON_SOURCES
                ${DROGON_SOURCES}
                orm_lib/src/postgresql_impl/PgConnection.cc)
            set(private_headers
                ${private_headers}
                orm_lib/src/postgresql_impl/PgConnection.h)
        endif (libpq_supports_batch)
    else(pg_FOUND)
        message(STATUS "Postgresql was not found.")
        option(USE_POSTGRESQL "Disable PostgreSQL" OFF)
    endif (pg_FOUND)
endif (BUILD_POSTGRESQL)

# MYSQL
if (BUILD_MYSQL)
    # Find mysql, only mariadb client library is supported
    find_package(MySQL QUIET)
    find_package(unofficial-libmariadb QUIET) #installed by command: vcpkg install
    if (MySQL_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE MySQL_lib)
        set(DROGON_FOUND_MYSQL TRUE)
    elseif (unofficial-libmariadb_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::libmariadb)
        set(DROGON_FOUND_MYSQL TRUE)
    endif ()

    if (DROGON_FOUND_MYSQL)
        message(STATUS "Ok! We find mariadb!")
        option(USE_MYSQL "Enable Mysql" ON)
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            orm_lib/src/mysql_impl/MysqlConnection.cc
            orm_lib/src/mysql_impl/MysqlResultImpl.cc)
        set(private_headers
            ${private_headers}
            orm_lib/src/mysql_impl/MysqlConnection.h
            orm_lib/src/mysql_impl/MysqlResultImpl.h)
    else (DROGON_FOUND_MYSQL)
        message(STATUS "MySql was not found.")
        option(USE_MYSQL "Disable Mysql" OFF)
    endif (DROGON_FOUND_MYSQL)
endif (BUILD_MYSQL)

# SQLITE
if (BUILD_SQLITE)
    # Find sqlite3.
    find_package(SQLite3 QUIET)
    find_package(unofficial-sqlite3 QUIET)      #installed by command: vcpkg install sqlite3
    if (SQLite3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE SQLite3_lib)
        set(DROGON_FOUND_SQLite3 TRUE)
    elseif (unofficial-sqlite3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::sqlite3::sqlite3)
        set(DROGON_FOUND_SQLite3 TRUE)
    endif ()

    if (DROGON_FOUND_SQLite3)
        message(STATUS "Ok! We find SQLite3!")
        option(USE_SQLITE3 "Enable Sqlite3" ON)
        set(DROGON_SOURCES 
            ${DROGON_SOURCES} 
            orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
            orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc)
        set(private_headers
            ${private_headers}
            orm_lib/src/sqlite3_impl/Sqlite3Connection.h
            orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.h)
    else (DROGON_FOUND_SQLite3)
        message(STATUS "SQLite3 was not found.")
        option(USE_SQLITE3 "Disable Sqlite3" OFF)
    endif (DROGON_FOUND_SQLite3)
endif (BUILD_SQLITE)

# dbs manager
if (pg_FOUND OR DROGON_FOUND_MYSQL OR DROGON_FOUND_SQLite3)
   set(DROGON_SOURCES ${DROGON_SOURCES} orm_lib/src/DbClientManager.cc)
else ()
   set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/DbClientManagerSkipped.cc)
endif ()

# redis
if (BUILD_REDIS)
    find_package(Hiredis QUIET)
    if (Hiredis_FOUND)
        message(STATUS "Found Hiredis")
        add_definitions(-DUSE_REDIS)
        target_link_libraries(${PROJECT_NAME} PRIVATE Hiredis_lib)
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            nosql_lib/redis/src/RedisClientImpl.cc
            nosql_lib/redis/src/RedisClientLockFree.cc
            nosql_lib/redis/src/RedisClientManager.cc
            nosql_lib/redis/src/RedisConnection.cc
            nosql_lib/redis/src/RedisResult.cc
            nosql_lib/redis/src/RedisTransactionImpl.cc)
        set(private_headers
            ${private_headers}
            nosql_lib/redis/src/RedisClientImpl.h
            nosql_lib/redis/src/RedisClientLockFree.h
            nosql_lib/redis/src/RedisConnection.h
            nosql_lib/redis/src/RedisTransactionImpl.h)
    else (Hiredis_FOUND)
        message(STATUS "Hiredis was not found.")
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            lib/src/RedisClientSkipped.cc
            lib/src/RedisResultSkipped.cc
            lib/src/RedisClientManagerSkipped.cc)
        set(private_headers
            ${private_headers}
            lib/src/RedisClientManager.h)
    endif (Hiredis_FOUND)
endif (BUILD_REDIS)

# drogon_ctl
if (BUILD_CTL)
    add_subdirectory(drogon_ctl)
endif (BUILD_CTL)

# examples
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif (BUILD_EXAMPLES)

# testing
if (BUILD_TESTING)
    message(STATUS "Building tests")
    
    # function ParseAndAddDrogonTests for all TestTarget 
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ParseAndAddDrogonTests.cmake)

    enable_testing()
    add_subdirectory(lib/tests)

    if (BUILD_ORM)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/tests)
    endif ()
    if (BUILD_POSTGRESQL AND pg_FOUND)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl/test)
    endif ()
    if (BUILD_MYSQL AND DROGON_FOUND_MYSQL)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/mysql_impl/test)
    endif ()
    if (BUILD_SQLITE AND DROGON_FOUND_SQLite3)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/sqlite3_impl/test)
    endif ()
    if (BUILD_REDIS)
        add_subdirectory(nosql_lib/redis/tests)
    endif ()
endif (BUILD_TESTING)

# ##############################################################################
# target sources, source group and include directories
# ##############################################################################
# target sources
target_sources(${PROJECT_NAME} PRIVATE
               ${DROGON_SOURCES}
               ${private_headers}
               ${DROGON_HEADERS}
               ${ORM_HEADERS}
               ${DROGON_UTIL_HEADERS}
               ${DROGON_PLUGIN_HEADERS}
               ${NOSQL_HEADERS})

# source group
source_group("Public API"
    FILES
    ${DROGON_HEADERS}
    ${ORM_HEADERS}
    ${DROGON_UTIL_HEADERS}
    ${DROGON_PLUGIN_HEADERS}
    ${NOSQL_HEADERS})
source_group("Private Headers"
             FILES
             ${private_headers})

# Include directories for build tree
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/inc>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/orm_lib/inc>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/nosql_lib/redis/inc>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/trantor>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/exports>)
if (WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/mman-win32>)
endif ()

# ##############################################################################
# config file
configure_file(${PROJECT_SOURCE_DIR}/cmake/templates/config.h.in
    ${PROJECT_BINARY_DIR}/drogon/config.h @ONLY)

# Export dll
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/exports/drogon/exports.h)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME Drogon)

# ##############################################################################
# Installation and package
# ##############################################################################
# Installation directories
include(GNUInstallDirs)
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE PATH "Installation directory for header files")
set(INSTALL_DROGON_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/Drogon CACHE PATH "Installation directory for cmake files")

# Include directories for install tree
target_include_directories(${PROJECT_NAME} PUBLIC $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>)

# Install bin/lib
install(TARGETS ${PROJECT_NAME}
    EXPORT DrogonTargets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)
# Install the export set for use with the install-tree
install(EXPORT DrogonTargets
    DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
    NAMESPACE Drogon::
    COMPONENT dev)

# Install header files
install(FILES ${DROGON_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon)
install(FILES ${ORM_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/orm)
install(FILES ${NOSQL_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/nosql)
install(FILES ${DROGON_UTIL_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils)
install(FILES ${DROGON_PLUGIN_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/plugins)

# Generating a Package Configuration File and Package Version File
include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/templates/DrogonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake
    INSTALL_DESTINATION ${INSTALL_DROGON_CMAKE_DIR})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DrogonConfigVersion.cmake
    VERSION ${DROGON_VERSION}
    COMPATIBILITY SameMajorVersion)

# Install the all cmake files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DrogonConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindUUID.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindJsoncpp.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindSQLite3.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindMySQL.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/Findpg.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindBrotli.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/Findcoz-profiler.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindHiredis.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindFilesystem.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DrogonUtilities.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ParseAndAddDrogonTests.cmake"
    DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
    COMPONENT dev)

# Pack
include(cmake/Packages.cmake)

# ##############################################################################
# Doxygen documentation
# ##############################################################################
find_package(Doxygen OPTIONAL_COMPONENTS dot dia)
if(DOXYGEN_FOUND)
  set(DOXYGEN_PROJECT_BRIEF "C++14/17-based HTTP application framework")
  set(DOXYGEN_OUTPUT_DIRECTORY docs/${PROJECT_NAME})
  set(DOXYGEN_GENERATE_LATEX NO)
  set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
  set(DOXYGEN_STRIP_FROM_INC_PATH ${PROJECT_SOURCE_DIR}/lib/inc
                                  ${PROJECT_SOURCE_DIR}/orm_lib/inc
                                  ${CMAKE_CURRENT_BINARY_DIR}/exports)
  set(DOXYGEN_EXAMPLE_PATTERNS *)

  if(WIN32)
    set(DOXYGEN_PREDEFINED _WIN32)
  endif(WIN32)

  doxygen_add_docs(doc_${PROJECT_NAME}
                   README.md
                   README.zh-CN.md
                   README.zh-TW.md
                   ChangeLog.md
                   CONTRIBUTING.md
                   ${DROGON_HEADERS}
                   ${DROGON_UTIL_HEADERS}
                   ${DROGON_PLUGIN_HEADERS}
                   ${ORM_HEADERS}
                   COMMENT "Generate documentation")

  if(NOT TARGET doc)
    add_custom_target(doc)
  endif()
  add_dependencies(doc doc_${PROJECT_NAME})

  if (BUILD_DOC)
    add_dependencies(${PROJECT_NAME} doc_${PROJECT_NAME})
    # Don't install twice, so limit to Debug (assume developer)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/${PROJECT_NAME}
            TYPE DOC
            CONFIGURATIONS Debug)
  endif(BUILD_DOC)
endif(DOXYGEN_FOUND)
