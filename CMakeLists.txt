cmake_minimum_required(VERSION 3.5)

project(drogon)

message(STATUS "compiler: " ${CMAKE_CXX_COMPILER_ID})

option(BUILD_CTL "Build drogon_ctl" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_ORM "Build orm" ON)
option(COZ_PROFILING "Use coz for profiling" OFF)
option(BUILD_SHARED_LIBS "Build drogon as a shared lib" OFF)
option(BUILD_DOC "Build Doxygen documentation" OFF)
option(BUILD_BROTLI "Build Brotli" ON)
option(BUILD_YAML_CONFIG "Build yaml config" ON)
option(USE_SUBMODULE "Use trantor as a submodule" ON)

include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_POSTGRESQL "Build with postgresql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(LIBPQ_BATCH_MODE "Use batch mode for libpq" ON "BUILD_POSTGRESQL" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_MYSQL "Build with mysql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_SQLITE "Build with sqlite3 support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_REDIS "Build with redis support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(USE_SPDLOG "Allow using the spdlog logging library" OFF "USE_SUBMODULE" OFF)

set(DROGON_MAJOR_VERSION 1)
set(DROGON_MINOR_VERSION 9)
set(DROGON_PATCH_VERSION 2)
set(DROGON_VERSION
    ${DROGON_MAJOR_VERSION}.${DROGON_MINOR_VERSION}.${DROGON_PATCH_VERSION})
set(DROGON_VERSION_STRING "${DROGON_VERSION}")

include(GNUInstallDirs)
# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE PATH "Installation directory for header files")
set(DEF_INSTALL_DROGON_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/Drogon)
set(INSTALL_DROGON_CMAKE_DIR ${DEF_INSTALL_DROGON_CMAKE_DIR}
    CACHE PATH "Installation directory for cmake files")

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # Force MSVC to use UTF-8 because that's what we use. Otherwise it uses
  # the default of whatever Windows sets and causes encoding issues.
  message(STATUS "You are using MSVC. Forceing to use UTF-8")
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
  if (MSVC_VERSION GREATER_EQUAL 1914)
    # Tells Visual Studio 2017 (15.7+) and newer to correctly set the value of the standard __cplusplus macro,
    # instead of leaving it to 199711L and settings the effective c++ version in _MSVC_LANG
    # Dropping support for older versions of VS would allow to only rely on __cplusplus
    add_compile_options("/Zc:__cplusplus")
  endif()
endif ()

add_library(${PROJECT_NAME})
if (BUILD_SHARED_LIBS)
    find_package(Threads)
    # set(BUILD_EXAMPLES FALSE)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
        "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" isSystemDir)
    if ("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")
    endif ("${isSystemDir}" STREQUAL "-1")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${DROGON_VERSION}
        SOVERSION ${DROGON_MAJOR_VERSION})
    target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)
    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PUBLIC rpcrt4 crypt32 advapi32 ws2_32)
        if (CMAKE_CXX_COMPILER_ID MATCHES MSVC)
            # Ignore MSVC C4251 and C4275 warning of exporting std objects with no dll export
            # We export class to facilitate maintenance, thus if you compile
            # drogon on windows as a shared library, you will need to use
            # exact same compiler for drogon and your app.
            target_compile_options(${PROJECT_NAME} PUBLIC /wd4251 /wd4275)
        endif ()
    endif ()
endif (BUILD_SHARED_LIBS)

if(USE_SPDLOG)
  find_package(spdlog CONFIG)
  if(spdlog_FOUND)
    message(STATUS "spdlog found!")
    target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog_header_only)
    target_compile_definitions(${PROJECT_NAME} PUBLIC DROGON_SPDLOG_SUPPORT SPDLOG_FMT_EXTERNAL FMT_HEADER_ONLY)
  endif(spdlog_FOUND)
endif(USE_SPDLOG)

if (NOT ${CMAKE_PLATFORM_NAME} STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif ()

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/exports/drogon/exports.h)

include(cmake/DrogonUtilities.cmake)
include(cmake/ParseAndAddDrogonTests.cmake)
include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    set(DROGON_CXX_STANDARD ${CMAKE_CXX_STANDARD})
elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(DROGON_CXX_STANDARD 20)
else ()
    set(DROGON_CXX_STANDARD 17)
endif ()
if(USE_SUBMODULE)
    target_include_directories(
            ${PROJECT_NAME}
            PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/trantor>)
endif()
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/inc>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/orm_lib/inc>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/nosql_lib/redis/inc>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/exports>
    $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>)

if (WIN32)
    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/mman-win32>)
endif (WIN32)

if(USE_SUBMODULE)
add_subdirectory(trantor)
target_link_libraries(${PROJECT_NAME} PUBLIC trantor)
else()
find_package(Trantor CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Trantor::Trantor)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Haiku")
    target_link_libraries(${PROJECT_NAME} PRIVATE network)
elseif (NOT WIN32 AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
elseif (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi ws2_32 iphlpapi)
endif ()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

find_package(Filesystem COMPONENTS Final)
if(CXX_FILESYSTEM_HAVE_FS)
    message(STATUS "Found std::filesystem")
else ()
    message(FATAL_ERROR "Not found std::filesystem, please use a newer compiler")
endif()

if (DROGON_CXX_STANDARD EQUAL 17)
    message(STATUS "use c++17")
    # Check for partial implementation of c++17 (Windows/OSX only?)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    try_compile(check_filesystem_path ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/check_has_std_filesystem_path.cc
        CXX_STANDARD 17)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE)
    if (NOT check_filesystem_path)
      message(FATAL_ERROR "The std::filesystem seems to be a partial implementation,"
        " please use a newer compiler")
    endif()
else ()
    message(STATUS "use c++20")
endif ()


option(HAS_STD_FILESYSTEM_PATH "use std::filesystem" ON)
# HACK: Needed to be compiled on Yocto Linux
if(TARGET std::filesystem)
  get_property(CAN_LINK_FS TARGET std::filesystem PROPERTY INTERFACE_LINK_LIBRARIES SET)
  if ( CAN_LINK_FS )
    target_link_libraries(${PROJECT_NAME} PUBLIC std::filesystem)
  endif()
endif()


# jsoncpp
find_package(Jsoncpp REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Jsoncpp_lib)
list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${JSONCPP_INCLUDE_DIRS})

# yamlcpp
if(BUILD_YAML_CONFIG)
    find_package(yaml-cpp QUIET)
    if(yaml-cpp_FOUND)
        if (NOT YAML_CPP_LIBRARIES)
            find_library(YAML_CPP_LINK_LIBRARY "yaml-cpp")
            if(NOT YAML_CPP_LINK_LIBRARY)
                message(STATUS "yaml-cpp not used")
            else()
                message(STATUS "yaml-cpp found ")
                target_link_libraries(${PROJECT_NAME} PUBLIC ${YAML_CPP_LINK_LIBRARY})
                target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_YAML_CPP)
            endif()
        else()
            message(STATUS "yaml-cpp found ")
            target_link_libraries(${PROJECT_NAME} PUBLIC ${YAML_CPP_LIBRARIES})
            target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_YAML_CPP)
        endif()
    else()
        message(STATUS "yaml-cpp not used")
    endif()
else()
    message(STATUS "yaml-cpp not used")
endif(BUILD_YAML_CONFIG)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
    AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD"
    AND NOT WIN32)
    find_package(UUID REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE UUID_lib)

    try_compile(normal_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/normal_uuid_lib_test.cc
        LINK_LIBRARIES UUID_lib
        OUTPUT_VARIABLE NORMAL_UUID_COMPILE_OUTPUT)
    try_compile(ossp_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/ossp_uuid_lib_test.cc
        LINK_LIBRARIES UUID_lib
        OUTPUT_VARIABLE OSSP_UUID_COMPILE_OUTPUT)
    if (normal_uuid)
        add_definitions(-DUSE_OSSP_UUID=0)
    elseif (ossp_uuid)
        add_definitions(-DUSE_OSSP_UUID=1)
    else ()
        message(FATAL_ERROR "uuid lib error:\n${NORMAL_UUID_COMPILE_OUTPUT}\n${OSSP_UUID_COMPILE_OUTPUT}")
    endif ()
endif (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
    AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD"
    AND NOT WIN32)

if (BUILD_BROTLI)
    find_package(Brotli)
    if (Brotli_FOUND)
        message(STATUS "Brotli found")
        add_definitions(-DUSE_BROTLI)
        target_link_libraries(${PROJECT_NAME} PRIVATE Brotli_lib)
    endif (Brotli_FOUND)
endif (BUILD_BROTLI)

set(DROGON_SOURCES
    lib/src/AOPAdvice.cc
    lib/src/AccessLogger.cc
    lib/src/CacheFile.cc
    lib/src/ConfigAdapterManager.cc
    lib/src/ConfigLoader.cc
    lib/src/Cookie.cc
    lib/src/DrClassMap.cc
    lib/src/DrTemplateBase.cc
    lib/src/FiltersFunction.cc
    lib/src/FixedWindowRateLimiter.cc
    lib/src/GlobalFilters.cc
    lib/src/Histogram.cc
    lib/src/Hodor.cc
    lib/src/HttpAppFrameworkImpl.cc
    lib/src/HttpBinder.cc
    lib/src/HttpClientImpl.cc
    lib/src/HttpConnectionLimit.cc
    lib/src/HttpControllerBinder.cc
    lib/src/HttpControllersRouter.cc
    lib/src/HttpFileImpl.cc
    lib/src/HttpFileUploadRequest.cc
    lib/src/HttpRequestImpl.cc
    lib/src/HttpRequestParser.cc
    lib/src/HttpResponseImpl.cc
    lib/src/HttpResponseParser.cc
    lib/src/HttpServer.cc
    lib/src/HttpUtils.cc
    lib/src/HttpViewData.cc
    lib/src/IntranetIpFilter.cc
    lib/src/JsonConfigAdapter.cc
    lib/src/ListenerManager.cc
    lib/src/LocalHostFilter.cc
    lib/src/MultiPart.cc
    lib/src/NotFound.cc
    lib/src/PluginsManager.cc
    lib/src/PromExporter.cc
    lib/src/RangeParser.cc
    lib/src/RateLimiter.cc
    lib/src/RealIpResolver.cc
    lib/src/SecureSSLRedirector.cc
    lib/src/Redirector.cc
    lib/src/SessionManager.cc
    lib/src/SlashRemover.cc
    lib/src/SlidingWindowRateLimiter.cc
    lib/src/StaticFileRouter.cc
    lib/src/TaskTimeoutFlag.cc
    lib/src/TokenBucketRateLimiter.cc
    lib/src/Utilities.cc
    lib/src/WebSocketClientImpl.cc
    lib/src/WebSocketConnectionImpl.cc
    lib/src/YamlConfigAdapter.cc
    lib/src/drogon_test.cc)
set(private_headers
    lib/src/AOPAdvice.h
    lib/src/CacheFile.h
    lib/src/ConfigLoader.h
    lib/src/ControllerBinderBase.h
    lib/src/FiltersFunction.h
    lib/src/HttpAppFrameworkImpl.h
    lib/src/HttpClientImpl.h
    lib/src/HttpConnectionLimit.h
    lib/src/HttpControllerBinder.h
    lib/src/HttpControllersRouter.h
    lib/src/HttpFileImpl.h
    lib/src/HttpFileUploadRequest.h
    lib/src/HttpMessageBody.h
    lib/src/HttpRequestImpl.h
    lib/src/HttpRequestParser.h
    lib/src/HttpResponseImpl.h
    lib/src/HttpResponseParser.h
    lib/src/HttpServer.h
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
    lib/src/FixedWindowRateLimiter.h
    lib/src/SlidingWindowRateLimiter.h
    lib/src/TokenBucketRateLimiter.h
    lib/src/ConfigAdapterManager.h
    lib/src/JsonConfigAdapter.h
    lib/src/YamlConfigAdapter.h
    lib/src/ConfigAdapter.h)

if (NOT WIN32)
    set(DROGON_SOURCES
        ${DROGON_SOURCES}
        lib/src/SharedLibManager.cc)
    set(private_headers
        ${private_headers}
        lib/src/SharedLibManager.h)
else (NOT WIN32)
    set(DROGON_SOURCES
        ${DROGON_SOURCES}
        third_party/mman-win32/mman.c)
    set(private_headers
        ${private_headers}
        third_party/mman-win32/mman.h)
endif (NOT WIN32)

if (BUILD_POSTGRESQL)
    # find postgres
    find_package(pg)
    if (pg_FOUND)
        message(STATUS "libpq inc path:" ${PG_INCLUDE_DIRS})
        message(STATUS "libpq lib:" ${PG_LIBRARIES})
        target_link_libraries(${PROJECT_NAME} PRIVATE pg_lib)
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc
            orm_lib/src/postgresql_impl/PgListener.cc)
        set(private_headers
            ${private_headers}
            orm_lib/src/postgresql_impl/PostgreSQLResultImpl.h
            orm_lib/src/postgresql_impl/PgListener.h)
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
    endif (pg_FOUND)
endif (BUILD_POSTGRESQL)

if (BUILD_MYSQL)
    # Find mysql, only mariadb client library is supported
    find_package(MySQL QUIET)
    find_package(unofficial-libmariadb QUIET)
    if (MySQL_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE MySQL_lib)
        set(DROGON_FOUND_MYSQL TRUE)
        set(MYSQL_LIB_NAME MySQL_lib)
    elseif (unofficial-libmariadb_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::libmariadb)
        set(DROGON_FOUND_MYSQL TRUE)
        set(MYSQL_LIB_NAME unofficial::libmariadb)
    endif ()

    if (DROGON_FOUND_MYSQL)
        message(STATUS "Ok! We find mariadb!")
        include(CheckLibraryExists)
        check_library_exists(${MYSQL_LIB_NAME} mysql_optionsv "" HAS_MYSQL_OPTIONSV)
        if (HAS_MYSQL_OPTIONSV)
            message(STATUS "Mariadb support mysql_optionsv")
            add_definitions(-DHAS_MYSQL_OPTIONSV)
        endif(HAS_MYSQL_OPTIONSV)

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
    endif (DROGON_FOUND_MYSQL)
endif (BUILD_MYSQL)

if (BUILD_SQLITE)
    # Find sqlite3.
    find_package(SQLite3 QUIET)
    find_package(unofficial-sqlite3 QUIET)
    if (SQLite3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE SQLite3_lib)
        set(DROGON_FOUND_SQLite3 TRUE)
    elseif (unofficial-sqlite3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::sqlite3::sqlite3)
        set(DROGON_FOUND_SQLite3 TRUE)
    endif ()

    if (DROGON_FOUND_SQLite3)
        message(STATUS "Ok! We find sqlite3!")
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
            orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc)
        set(private_headers
            ${private_headers}
            orm_lib/src/sqlite3_impl/Sqlite3Connection.h
            orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.h)
    else (DROGON_FOUND_SQLite3)
        message(STATUS "sqlite3 was not found.")
    endif (DROGON_FOUND_SQLite3)
endif (BUILD_SQLITE)

if (BUILD_REDIS)
    find_package(Hiredis)
    if (Hiredis_FOUND)
        add_definitions(-DUSE_REDIS)
        target_link_libraries(${PROJECT_NAME} PRIVATE Hiredis_lib)
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            nosql_lib/redis/src/RedisClientImpl.cc
            nosql_lib/redis/src/RedisClientLockFree.cc
            nosql_lib/redis/src/RedisClientManager.cc
            nosql_lib/redis/src/RedisConnection.cc
            nosql_lib/redis/src/RedisResult.cc
            nosql_lib/redis/src/RedisTransactionImpl.cc
            nosql_lib/redis/src/SubscribeContext.cc
            nosql_lib/redis/src/RedisSubscriberImpl.cc)
        set(private_headers
            ${private_headers}
            nosql_lib/redis/src/RedisClientImpl.h
            nosql_lib/redis/src/RedisClientLockFree.h
            nosql_lib/redis/src/RedisConnection.h
            nosql_lib/redis/src/RedisTransactionImpl.h
            nosql_lib/redis/src/SubscribeContext.h
            nosql_lib/redis/src/RedisSubscriberImpl.h)

    endif (Hiredis_FOUND)
endif (BUILD_REDIS)

if (NOT Hiredis_FOUND)
    set(DROGON_SOURCES
        ${DROGON_SOURCES}
        lib/src/RedisClientSkipped.cc
        lib/src/RedisResultSkipped.cc
        lib/src/RedisClientManagerSkipped.cc)
    set(private_headers
        ${private_headers}
        lib/src/RedisClientManager.h)
endif (NOT Hiredis_FOUND)

if (BUILD_TESTING)
    add_subdirectory(nosql_lib/redis/tests)
endif (BUILD_TESTING)

find_package(ZLIB REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)

execute_process(COMMAND "git" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_SHA1
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/version.h.in"
    "${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h" @ONLY)

if (DROGON_CXX_STANDARD EQUAL 20)
    option(USE_COROUTINE "Enable C++20 coroutine support" ON)
else (DROGON_CXX_STANDARD EQUAL 20)
    option(USE_COROUTINE "Enable C++20 coroutine support" OFF)
endif (DROGON_CXX_STANDARD EQUAL 20)

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif (BUILD_EXAMPLES)

if (BUILD_CTL)
    add_subdirectory(drogon_ctl)
endif (BUILD_CTL)

if (COZ_PROFILING)
    find_package(coz-profiler REQUIRED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOZ_PROFILING=1)
    # If linked will not need to be ran with `coz run --- [executable]` to run the
    # profiler, but drogon_ctl currently won't build because it doesn't find debug
    # information while trying to generate it's own sources
    # target_link_libraries(${PROJECT_NAME} PUBLIC coz::coz)
    target_include_directories(${PROJECT_NAME} PUBLIC ${COZ_INCLUDE_DIRS})
endif (COZ_PROFILING)

set(DROGON_SOURCES
    ${DROGON_SOURCES}
    orm_lib/src/ArrayParser.cc
    orm_lib/src/Criteria.cc
    orm_lib/src/DbClient.cc
    orm_lib/src/DbClientImpl.cc
    orm_lib/src/DbClientLockFree.cc
    orm_lib/src/DbConnection.cc
    orm_lib/src/DbListener.cc
    orm_lib/src/Exception.cc
    orm_lib/src/Field.cc
    orm_lib/src/Result.cc
    orm_lib/src/Row.cc
    orm_lib/src/SqlBinder.cc
    orm_lib/src/TransactionImpl.cc
    orm_lib/src/RestfulController.cc)
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
    lib/inc/drogon/RateLimiter.h
    ${CMAKE_CURRENT_BINARY_DIR}/exports/drogon/exports.h)
set(private_headers
    ${private_headers}
    lib/src/DbClientManager.h
    orm_lib/src/DbClientImpl.h
    orm_lib/src/DbConnection.h
    orm_lib/src/ResultImpl.h
    orm_lib/src/TransactionImpl.h)
if (pg_FOUND OR DROGON_FOUND_MYSQL OR DROGON_FOUND_SQLite3)
    set(DROGON_SOURCES
        ${DROGON_SOURCES}
        orm_lib/src/DbClientManager.cc)
else (pg_FOUND OR DROGON_FOUND_MYSQL OR DROGON_FOUND_SQLite3)
    set(DROGON_SOURCES
        ${DROGON_SOURCES}
        lib/src/DbClientManagerSkipped.cc)
endif (pg_FOUND OR DROGON_FOUND_MYSQL OR DROGON_FOUND_SQLite3)

set_target_properties(${PROJECT_NAME}
    PROPERTIES CXX_STANDARD ${DROGON_CXX_STANDARD})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME Drogon)

if (pg_FOUND OR DROGON_FOUND_MYSQL OR DROGON_FOUND_SQLite3)
    if (pg_FOUND)
        option(USE_POSTGRESQL "Enable PostgreSQL" ON)
    else (pg_FOUND)
        option(USE_POSTGRESQL "Disable PostgreSQL" OFF)
    endif (pg_FOUND)

    if (DROGON_FOUND_MYSQL)
        option(USE_MYSQL "Enable Mysql" ON)
    else (DROGON_FOUND_MYSQL)
        option(USE_MYSQL "Disable Mysql" OFF)
    endif (DROGON_FOUND_MYSQL)

    if (DROGON_FOUND_SQLite3)
        option(USE_SQLITE3 "Enable Sqlite3" ON)
    else (DROGON_FOUND_SQLite3)
        option(USE_SQLITE3 "Disable Sqlite3" OFF)
    endif (DROGON_FOUND_SQLite3)
endif (pg_FOUND OR DROGON_FOUND_MYSQL OR DROGON_FOUND_SQLite3)

get_filename_component(COMPILER_COMMAND ${CMAKE_CXX_COMPILER} NAME)
set(COMPILER_ID ${CMAKE_CXX_COMPILER_ID})

if (CMAKE_BUILD_TYPE)
    string(TOLOWER ${CMAKE_BUILD_TYPE} _type)
    if (_type STREQUAL release)
        set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -std=c++")
    elseif (_type STREQUAL debug)
        set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++")
    else ()
        set(COMPILATION_FLAGS "-std=c++")
    endif ()
else (CMAKE_BUILD_TYPE)
    set(COMPILATION_FLAGS "-std=c++")
endif (CMAKE_BUILD_TYPE)

list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW
    "${CMAKE_INSTALL_PREFIX}/${INSTALL_INCLUDE_DIR}")
list(REMOVE_DUPLICATES INCLUDE_DIRS_FOR_DYNAMIC_VIEW)
set(INS_STRING "")
foreach (loop_var ${INCLUDE_DIRS_FOR_DYNAMIC_VIEW})
    set(INS_STRING "${INS_STRING} -I${loop_var}")
endforeach (loop_var)

set(INCLUDING_DIRS ${INS_STRING})

configure_file(${PROJECT_SOURCE_DIR}/cmake/templates/config.h.in
    ${PROJECT_BINARY_DIR}/drogon/config.h @ONLY)

if (BUILD_TESTING)
    message(STATUS "Building tests")
    enable_testing()
    add_subdirectory(lib/tests)
    if (pg_FOUND)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl/test)
    endif (pg_FOUND)
    if (DROGON_FOUND_MYSQL)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/mysql_impl/test)
    endif (DROGON_FOUND_MYSQL)
    if (DROGON_FOUND_SQLite3)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/sqlite3_impl/test)
    endif (DROGON_FOUND_SQLite3)
    add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/tests)
endif (BUILD_TESTING)

# Installation

install(TARGETS ${PROJECT_NAME}
    EXPORT DrogonTargets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)

install(FILES ${DROGON_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon)

set(ORM_HEADERS
    orm_lib/inc/drogon/orm/ArrayParser.h
    orm_lib/inc/drogon/orm/BaseBuilder.h
    orm_lib/inc/drogon/orm/Criteria.h
    orm_lib/inc/drogon/orm/DbClient.h
    orm_lib/inc/drogon/orm/DbListener.h
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
install(FILES ${ORM_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/orm)

set(NOSQL_HEADERS
    nosql_lib/redis/inc/drogon/nosql/RedisClient.h
    nosql_lib/redis/inc/drogon/nosql/RedisResult.h
    nosql_lib/redis/inc/drogon/nosql/RedisSubscriber.h
    nosql_lib/redis/inc/drogon/nosql/RedisException.h)
install(FILES ${NOSQL_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/nosql)

set(DROGON_UTIL_HEADERS
    lib/inc/drogon/utils/coroutine.h
    lib/inc/drogon/utils/FunctionTraits.h
    lib/inc/drogon/utils/HttpConstraint.h
    lib/inc/drogon/utils/OStringStream.h
    lib/inc/drogon/utils/Utilities.h
    lib/inc/drogon/utils/monitoring.h)
install(FILES ${DROGON_UTIL_HEADERS}
    DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils)

set(DROGON_MONITORING_HEADERS
    lib/inc/drogon/utils/monitoring/Counter.h
    lib/inc/drogon/utils/monitoring/Metric.h
    lib/inc/drogon/utils/monitoring/Registry.h
    lib/inc/drogon/utils/monitoring/Collector.h
    lib/inc/drogon/utils/monitoring/Sample.h
    lib/inc/drogon/utils/monitoring/Gauge.h
    lib/inc/drogon/utils/monitoring/Histogram.h)

install(FILES ${DROGON_MONITORING_HEADERS}
    DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils/monitoring)

set(DROGON_PLUGIN_HEADERS
    lib/inc/drogon/plugins/Plugin.h
    lib/inc/drogon/plugins/Redirector.h
    lib/inc/drogon/plugins/SecureSSLRedirector.h
    lib/inc/drogon/plugins/AccessLogger.h
    lib/inc/drogon/plugins/RealIpResolver.h
    lib/inc/drogon/plugins/Hodor.h
    lib/inc/drogon/plugins/SlashRemover.h
    lib/inc/drogon/plugins/GlobalFilters.h
    lib/inc/drogon/plugins/PromExporter.h)

install(FILES ${DROGON_PLUGIN_HEADERS}
    DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/plugins)

target_sources(${PROJECT_NAME} PRIVATE
               ${DROGON_SOURCES}
               ${private_headers}
               ${DROGON_HEADERS}
               ${ORM_HEADERS}
               ${DROGON_UTIL_HEADERS}
               ${DROGON_PLUGIN_HEADERS}
               ${NOSQL_HEADERS}
               ${DROGON_MONITORING_HEADERS})

source_group("Public API"
    FILES
    ${DROGON_HEADERS}
    ${ORM_HEADERS}
    ${DROGON_UTIL_HEADERS}
    ${DROGON_PLUGIN_HEADERS}
    ${NOSQL_HEADERS}
    ${DROGON_MONITORING_HEADERS})
source_group("Private Headers"
             FILES
             ${private_headers})

# Export the package for use from the build-tree (this registers the build-tree
# with a global cmake-registry) export(PACKAGE Drogon)

include(CMakePackageConfigHelpers)
# ... for the install tree
configure_package_config_file(
    cmake/templates/DrogonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake
    INSTALL_DESTINATION
    ${INSTALL_DROGON_CMAKE_DIR})

# version
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DrogonConfigVersion.cmake
    VERSION ${DROGON_VERSION}
    COMPATIBILITY SameMajorVersion)

# Install the DrogonConfig.cmake and DrogonConfigVersion.cmake
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

# Install the export set for use with the install-tree
install(EXPORT DrogonTargets
    DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
    NAMESPACE Drogon::
    COMPONENT dev)

# Doxygen documentation
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

include(cmake/Packages.cmake)
