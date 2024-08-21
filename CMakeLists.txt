CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

PROJECT(drogon)

MESSAGE(STATUS "compiler: " ${CMAKE_CXX_COMPILER_ID})

OPTION(BUILD_CTL "Build drogon_ctl" ON)
OPTION(BUILD_EXAMPLES "Build examples" ON)
OPTION(BUILD_ORM "Build orm" ON)
OPTION(COZ_PROFILING "Use coz for profiling" OFF)
OPTION(BUILD_SHARED_LIBS "Build drogon as a shared lib" OFF)
OPTION(BUILD_DOC "Build Doxygen documentation" OFF)
OPTION(BUILD_BROTLI "Build Brotli" ON)
OPTION(BUILD_YAML_CONFIG "Build yaml config" ON)
OPTION(USE_SUBMODULE "Use trantor as a submodule" ON)
OPTION(USE_STATIC_LIBS_ONLY "Use only static libraries as dependencies" OFF)

INCLUDE(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_POSTGRESQL "Build with postgresql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(LIBPQ_BATCH_MODE "Use batch mode for libpq" ON "BUILD_POSTGRESQL" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_MYSQL "Build with mysql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_SQLITE "Build with sqlite3 support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_REDIS "Build with redis support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(USE_SPDLOG "Allow using the spdlog logging library" OFF "USE_SUBMODULE" OFF)

SET(DROGON_MAJOR_VERSION 1)
SET(DROGON_MINOR_VERSION 9)
SET(DROGON_PATCH_VERSION 6)
SET(DROGON_VERSION ${DROGON_MAJOR_VERSION}.${DROGON_MINOR_VERSION}.${DROGON_PATCH_VERSION})
SET(DROGON_VERSION_STRING "${DROGON_VERSION}")

INCLUDE(GNUInstallDirs)
# Offer the user the choice of overriding the installation directories
SET(INSTALL_LIB_DIR
    ${CMAKE_INSTALL_LIBDIR}
    CACHE PATH "Installation directory for libraries"
)
SET(INSTALL_BIN_DIR
    ${CMAKE_INSTALL_BINDIR}
    CACHE PATH "Installation directory for executables"
)
SET(INSTALL_INCLUDE_DIR
    ${CMAKE_INSTALL_INCLUDEDIR}
    CACHE PATH "Installation directory for header files"
)
SET(DEF_INSTALL_DROGON_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/Drogon)
SET(INSTALL_DROGON_CMAKE_DIR
    ${DEF_INSTALL_DROGON_CMAKE_DIR}
    CACHE PATH "Installation directory for cmake files"
)

IF(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Force MSVC to use UTF-8 because that's what we use. Otherwise it uses the default of whatever Windows sets and
    # causes encoding issues.
    MESSAGE(STATUS "You are using MSVC. Forceing to use UTF-8")
    ADD_COMPILE_OPTIONS("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    ADD_COMPILE_OPTIONS("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    IF(MSVC_VERSION GREATER_EQUAL 1914)
        # Tells Visual Studio 2017 (15.7+) and newer to correctly set the value of the standard __cplusplus macro,
        # instead of leaving it to 199711L and settings the effective c++ version in _MSVC_LANG Dropping support for
        # older versions of VS would allow to only rely on __cplusplus
        ADD_COMPILE_OPTIONS("/Zc:__cplusplus")
    ENDIF()
ENDIF()

ADD_LIBRARY(${PROJECT_NAME})
IF(BUILD_SHARED_LIBS)
    FIND_PACKAGE(Threads)
    # set(BUILD_EXAMPLES FALSE)
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
        SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")
    ENDIF("${isSystemDir}" STREQUAL "-1")
    SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES VERSION ${DROGON_VERSION} SOVERSION ${DROGON_MAJOR_VERSION})
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Threads::Threads)
    IF(WIN32)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC rpcrt4 crypt32 advapi32 ws2_32)
        IF(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
            # Ignore MSVC C4251 and C4275 warning of exporting std objects with no dll export We export class to
            # facilitate maintenance, thus if you compile drogon on windows as a shared library, you will need to use
            # exact same compiler for drogon and your app.
            TARGET_COMPILE_OPTIONS(${PROJECT_NAME} PUBLIC /wd4251 /wd4275)
        ENDIF()
    ENDIF()
ENDIF(BUILD_SHARED_LIBS)

IF(USE_STATIC_LIBS_ONLY)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
ENDIF(USE_STATIC_LIBS_ONLY)

IF(USE_SPDLOG)
    FIND_PACKAGE(spdlog CONFIG)
    IF(spdlog_FOUND)
        MESSAGE(STATUS "spdlog found!")
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC spdlog::spdlog_header_only)
        TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC DROGON_SPDLOG_SUPPORT SPDLOG_FMT_EXTERNAL FMT_HEADER_ONLY)
    ENDIF(spdlog_FOUND)
ENDIF(USE_SPDLOG)

IF(NOT ${CMAKE_PLATFORM_NAME} STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID MATCHES GNU)
    TARGET_COMPILE_OPTIONS(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
ENDIF()

INCLUDE(GenerateExportHeader)
GENERATE_EXPORT_HEADER(${PROJECT_NAME} EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/exports/drogon/exports.h)

INCLUDE(cmake/DrogonUtilities.cmake)
INCLUDE(cmake/ParseAndAddDrogonTests.cmake)
INCLUDE(CheckIncludeFileCXX)

CHECK_INCLUDE_FILE_CXX(any HAS_ANY)
CHECK_INCLUDE_FILE_CXX(string_view HAS_STRING_VIEW)
CHECK_INCLUDE_FILE_CXX(coroutine HAS_COROUTINE)
IF(NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    SET(DROGON_CXX_STANDARD ${CMAKE_CXX_STANDARD})
ELSEIF(
    HAS_ANY
    AND HAS_STRING_VIEW
    AND HAS_COROUTINE
)
    SET(DROGON_CXX_STANDARD 20)
ELSE()
    SET(DROGON_CXX_STANDARD 17)
ENDIF()
IF(USE_SUBMODULE)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/trantor>)
ENDIF()
TARGET_INCLUDE_DIRECTORIES(
    ${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/inc> $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/orm_lib/inc>
           $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/nosql_lib/redis/inc> $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/exports> $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>
)

IF(WIN32)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/mman-win32>)
ENDIF(WIN32)

IF(USE_SUBMODULE)
    ADD_SUBDIRECTORY(trantor)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC trantor)
ELSE()
    FIND_PACKAGE(Trantor CONFIG REQUIRED)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Trantor::Trantor)
ENDIF()

IF(${CMAKE_SYSTEM_NAME} STREQUAL "Haiku")
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE network)
ELSEIF(NOT WIN32 AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE dl)
ELSEIF(WIN32)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE shlwapi ws2_32 iphlpapi)
ENDIF()

LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

FIND_PACKAGE(Filesystem COMPONENTS Final)
IF(CXX_FILESYSTEM_HAVE_FS)
    MESSAGE(STATUS "Found std::filesystem")
ELSE()
    MESSAGE(FATAL_ERROR "Not found std::filesystem, please use a newer compiler")
ENDIF()

IF(DROGON_CXX_STANDARD EQUAL 17)
    MESSAGE(STATUS "use c++17")
    # Check for partial implementation of c++17 (Windows/OSX only?)
    SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    TRY_COMPILE(
        check_filesystem_path ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/check_has_std_filesystem_path.cc CXX_STANDARD 17
    )
    SET(CMAKE_TRY_COMPILE_TARGET_TYPE)
    IF(NOT check_filesystem_path)
        MESSAGE(FATAL_ERROR "The std::filesystem seems to be a partial implementation," " please use a newer compiler")
    ENDIF()
ELSE()
    MESSAGE(STATUS "use c++20")
ENDIF()

OPTION(HAS_STD_FILESYSTEM_PATH "use std::filesystem" ON)
# HACK: Needed to be compiled on Yocto Linux
IF(TARGET std::filesystem)
    GET_PROPERTY(
        CAN_LINK_FS
        TARGET std::filesystem
        PROPERTY INTERFACE_LINK_LIBRARIES
        SET
    )
    IF(CAN_LINK_FS)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC std::filesystem)
    ENDIF()
ENDIF()

# jsoncpp
FIND_PACKAGE(Jsoncpp REQUIRED)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Jsoncpp_lib)
LIST(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${JSONCPP_INCLUDE_DIRS})

# yamlcpp
IF(BUILD_YAML_CONFIG)
    FIND_PACKAGE(yaml-cpp QUIET)
    IF(yaml-cpp_FOUND)
        IF(NOT YAML_CPP_LIBRARIES)
            FIND_LIBRARY(YAML_CPP_LINK_LIBRARY "yaml-cpp")
            IF(NOT YAML_CPP_LINK_LIBRARY)
                MESSAGE(STATUS "yaml-cpp not used")
            ELSE()
                MESSAGE(STATUS "yaml-cpp found ")
                TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${YAML_CPP_LINK_LIBRARY})
                TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC HAS_YAML_CPP)
            ENDIF()
        ELSE()
            MESSAGE(STATUS "yaml-cpp found ")
            TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${YAML_CPP_LIBRARIES})
            TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC HAS_YAML_CPP)
        ENDIF()
    ELSE()
        MESSAGE(STATUS "yaml-cpp not used")
    ENDIF()
ELSE()
    MESSAGE(STATUS "yaml-cpp not used")
ENDIF(BUILD_YAML_CONFIG)

IF(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
   AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD"
   AND NOT WIN32
)
    FIND_PACKAGE(UUID REQUIRED)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE UUID_lib)

    TRY_COMPILE(
        normal_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/normal_uuid_lib_test.cc
        LINK_LIBRARIES UUID_lib
        OUTPUT_VARIABLE NORMAL_UUID_COMPILE_OUTPUT
    )
    TRY_COMPILE(
        ossp_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${PROJECT_SOURCE_DIR}/cmake/tests/ossp_uuid_lib_test.cc
        LINK_LIBRARIES UUID_lib
        OUTPUT_VARIABLE OSSP_UUID_COMPILE_OUTPUT
    )
    IF(normal_uuid)
        ADD_DEFINITIONS(-DUSE_OSSP_UUID=0)
    ELSEIF(ossp_uuid)
        ADD_DEFINITIONS(-DUSE_OSSP_UUID=1)
    ELSE()
        MESSAGE(FATAL_ERROR "uuid lib error:\n${NORMAL_UUID_COMPILE_OUTPUT}\n${OSSP_UUID_COMPILE_OUTPUT}")
    ENDIF()
ENDIF(
    NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
    AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD"
    AND NOT WIN32
)

IF(BUILD_BROTLI)
    FIND_PACKAGE(Brotli)
    IF(Brotli_FOUND)
        MESSAGE(STATUS "Brotli found")
        ADD_DEFINITIONS(-DUSE_BROTLI)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE Brotli_lib)
    ENDIF(Brotli_FOUND)
ENDIF(BUILD_BROTLI)

SET(DROGON_SOURCES
    lib/src/AOPAdvice.cc
    lib/src/AccessLogger.cc
    lib/src/CacheFile.cc
    lib/src/ConfigAdapterManager.cc
    lib/src/ConfigLoader.cc
    lib/src/Cookie.cc
    lib/src/DrClassMap.cc
    lib/src/DrTemplateBase.cc
    lib/src/MiddlewaresFunction.cc
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
    lib/src/RequestStream.cc
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
    lib/src/MultipartStreamParser.cc
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
    lib/src/drogon_test.cc
)
SET(private_headers
    lib/src/AOPAdvice.h
    lib/src/CacheFile.h
    lib/src/ConfigLoader.h
    lib/src/ControllerBinderBase.h
    lib/src/MiddlewaresFunction.h
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
    lib/src/ConfigAdapter.h
    lib/src/MultipartStreamParser.h
)

IF(NOT WIN32)
    SET(DROGON_SOURCES ${DROGON_SOURCES} lib/src/SharedLibManager.cc)
    SET(private_headers ${private_headers} lib/src/SharedLibManager.h)
ELSE(NOT WIN32)
    SET(DROGON_SOURCES ${DROGON_SOURCES} third_party/mman-win32/mman.c)
    SET(private_headers ${private_headers} third_party/mman-win32/mman.h)
ENDIF(NOT WIN32)

IF(BUILD_POSTGRESQL)
    # find postgres
    FIND_PACKAGE(pg)
    IF(pg_FOUND)
        MESSAGE(STATUS "libpq inc path:" ${PG_INCLUDE_DIRS})
        MESSAGE(STATUS "libpq lib:" ${PG_LIBRARIES})
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE pg_lib)
        SET(DROGON_SOURCES ${DROGON_SOURCES} orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc
                           orm_lib/src/postgresql_impl/PgListener.cc
        )
        SET(private_headers ${private_headers} orm_lib/src/postgresql_impl/PostgreSQLResultImpl.h
                            orm_lib/src/postgresql_impl/PgListener.h
        )
        IF(LIBPQ_BATCH_MODE)
            TRY_COMPILE(
                libpq_supports_batch ${CMAKE_BINARY_DIR}/cmaketest
                ${PROJECT_SOURCE_DIR}/cmake/tests/test_libpq_batch_mode.cc
                LINK_LIBRARIES ${PostgreSQL_LIBRARIES}
                CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PostgreSQL_INCLUDE_DIR}"
            )
        ENDIF(LIBPQ_BATCH_MODE)
        IF(libpq_supports_batch)
            MESSAGE(STATUS "The libpq supports batch mode")
            OPTION(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" ON)
            SET(DROGON_SOURCES ${DROGON_SOURCES} orm_lib/src/postgresql_impl/PgBatchConnection.cc)
        ELSE(libpq_supports_batch)
            OPTION(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" OFF)
            SET(DROGON_SOURCES ${DROGON_SOURCES} orm_lib/src/postgresql_impl/PgConnection.cc)
            SET(private_headers ${private_headers} orm_lib/src/postgresql_impl/PgConnection.h)
        ENDIF(libpq_supports_batch)
    ENDIF(pg_FOUND)
ENDIF(BUILD_POSTGRESQL)

IF(BUILD_MYSQL)
    # Find mysql, only mariadb client library is supported
    FIND_PACKAGE(MySQL QUIET)
    FIND_PACKAGE(unofficial-libmariadb QUIET)
    IF(MySQL_FOUND)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE MySQL_lib)
        SET(DROGON_FOUND_MYSQL TRUE)
        SET(MYSQL_LIB_NAME MySQL_lib)
    ELSEIF(unofficial-libmariadb_FOUND)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE unofficial::libmariadb)
        SET(DROGON_FOUND_MYSQL TRUE)
        SET(MYSQL_LIB_NAME unofficial::libmariadb)
    ENDIF()

    IF(DROGON_FOUND_MYSQL)
        MESSAGE(STATUS "Ok! We find mariadb!")
        INCLUDE(CheckLibraryExists)
        CHECK_LIBRARY_EXISTS(${MYSQL_LIB_NAME} mysql_optionsv "" HAS_MYSQL_OPTIONSV)
        IF(HAS_MYSQL_OPTIONSV)
            MESSAGE(STATUS "Mariadb support mysql_optionsv")
            ADD_DEFINITIONS(-DHAS_MYSQL_OPTIONSV)
        ENDIF(HAS_MYSQL_OPTIONSV)

        SET(DROGON_SOURCES ${DROGON_SOURCES} orm_lib/src/mysql_impl/MysqlConnection.cc
                           orm_lib/src/mysql_impl/MysqlResultImpl.cc
        )
        SET(private_headers ${private_headers} orm_lib/src/mysql_impl/MysqlConnection.h
                            orm_lib/src/mysql_impl/MysqlResultImpl.h
        )
    ELSE(DROGON_FOUND_MYSQL)
        MESSAGE(STATUS "MySql was not found.")
    ENDIF(DROGON_FOUND_MYSQL)
ENDIF(BUILD_MYSQL)

IF(BUILD_SQLITE)
    # Find sqlite3.
    FIND_PACKAGE(SQLite3 QUIET)
    FIND_PACKAGE(unofficial-sqlite3 QUIET)
    IF(SQLite3_FOUND)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE SQLite3_lib)
        SET(DROGON_FOUND_SQLite3 TRUE)
    ELSEIF(unofficial-sqlite3_FOUND)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE unofficial::sqlite3::sqlite3)
        SET(DROGON_FOUND_SQLite3 TRUE)
    ENDIF()

    IF(DROGON_FOUND_SQLite3)
        MESSAGE(STATUS "Ok! We find sqlite3!")
        SET(DROGON_SOURCES ${DROGON_SOURCES} orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
                           orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc
        )
        SET(private_headers ${private_headers} orm_lib/src/sqlite3_impl/Sqlite3Connection.h
                            orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.h
        )
    ELSE(DROGON_FOUND_SQLite3)
        MESSAGE(STATUS "sqlite3 was not found.")
    ENDIF(DROGON_FOUND_SQLite3)
ENDIF(BUILD_SQLITE)

IF(BUILD_REDIS)
    FIND_PACKAGE(Hiredis)
    IF(Hiredis_FOUND)
        ADD_DEFINITIONS(-DUSE_REDIS)
        TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE Hiredis_lib)
        SET(DROGON_SOURCES
            ${DROGON_SOURCES}
            nosql_lib/redis/src/RedisClientImpl.cc
            nosql_lib/redis/src/RedisClientLockFree.cc
            nosql_lib/redis/src/RedisClientManager.cc
            nosql_lib/redis/src/RedisConnection.cc
            nosql_lib/redis/src/RedisResult.cc
            nosql_lib/redis/src/RedisTransactionImpl.cc
            nosql_lib/redis/src/SubscribeContext.cc
            nosql_lib/redis/src/RedisSubscriberImpl.cc
        )
        SET(private_headers
            ${private_headers}
            nosql_lib/redis/src/RedisClientImpl.h
            nosql_lib/redis/src/RedisClientLockFree.h
            nosql_lib/redis/src/RedisConnection.h
            nosql_lib/redis/src/RedisTransactionImpl.h
            nosql_lib/redis/src/SubscribeContext.h
            nosql_lib/redis/src/RedisSubscriberImpl.h
        )

    ENDIF(Hiredis_FOUND)
ENDIF(BUILD_REDIS)

IF(NOT Hiredis_FOUND)
    SET(DROGON_SOURCES ${DROGON_SOURCES} lib/src/RedisClientSkipped.cc lib/src/RedisResultSkipped.cc
                       lib/src/RedisClientManagerSkipped.cc
    )
    SET(private_headers ${private_headers} lib/src/RedisClientManager.h)
ENDIF(NOT Hiredis_FOUND)

IF(BUILD_TESTING)
    ADD_SUBDIRECTORY(nosql_lib/redis/tests)
ENDIF(BUILD_TESTING)

FIND_PACKAGE(ZLIB REQUIRED)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)

EXECUTE_PROCESS(
    COMMAND "git" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_SHA1
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
)
CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/cmake/templates/version.h.in" "${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h" @ONLY
)

IF(DROGON_CXX_STANDARD EQUAL 20)
    OPTION(USE_COROUTINE "Enable C++20 coroutine support" ON)
ELSE(DROGON_CXX_STANDARD EQUAL 20)
    OPTION(USE_COROUTINE "Enable C++20 coroutine support" OFF)
ENDIF(DROGON_CXX_STANDARD EQUAL 20)

IF(BUILD_EXAMPLES)
    ADD_SUBDIRECTORY(examples)
ENDIF(BUILD_EXAMPLES)

IF(BUILD_CTL)
    ADD_SUBDIRECTORY(drogon_ctl)
ENDIF(BUILD_CTL)

IF(COZ_PROFILING)
    FIND_PACKAGE(coz-profiler REQUIRED)
    TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE -DCOZ_PROFILING=1)
    # If linked will not need to be ran with `coz run --- [executable]` to run the profiler, but drogon_ctl currently
    # won't build because it doesn't find debug information while trying to generate it's own sources
    # target_link_libraries(${PROJECT_NAME} PUBLIC coz::coz)
    TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${COZ_INCLUDE_DIRS})
ENDIF(COZ_PROFILING)

SET(DROGON_SOURCES
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
    orm_lib/src/RestfulController.cc
)
SET(DROGON_HEADERS
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
    lib/inc/drogon/HttpMiddleware.h
    lib/inc/drogon/HttpRequest.h
    lib/inc/drogon/RequestStream.h
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
    ${CMAKE_CURRENT_BINARY_DIR}/exports/drogon/exports.h
)
SET(private_headers ${private_headers} lib/src/DbClientManager.h orm_lib/src/DbClientImpl.h orm_lib/src/DbConnection.h
                    orm_lib/src/ResultImpl.h orm_lib/src/TransactionImpl.h
)
IF(pg_FOUND
   OR DROGON_FOUND_MYSQL
   OR DROGON_FOUND_SQLite3
)
    SET(DROGON_SOURCES ${DROGON_SOURCES} orm_lib/src/DbClientManager.cc)
ELSE(
    pg_FOUND
    OR DROGON_FOUND_MYSQL
    OR DROGON_FOUND_SQLite3
)
    SET(DROGON_SOURCES ${DROGON_SOURCES} lib/src/DbClientManagerSkipped.cc)
ENDIF(
    pg_FOUND
    OR DROGON_FOUND_MYSQL
    OR DROGON_FOUND_SQLite3
)

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES CXX_STANDARD ${DROGON_CXX_STANDARD})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES EXPORT_NAME Drogon)

IF(pg_FOUND
   OR DROGON_FOUND_MYSQL
   OR DROGON_FOUND_SQLite3
)
    IF(pg_FOUND)
        OPTION(USE_POSTGRESQL "Enable PostgreSQL" ON)
    ELSE(pg_FOUND)
        OPTION(USE_POSTGRESQL "Disable PostgreSQL" OFF)
    ENDIF(pg_FOUND)

    IF(DROGON_FOUND_MYSQL)
        OPTION(USE_MYSQL "Enable Mysql" ON)
    ELSE(DROGON_FOUND_MYSQL)
        OPTION(USE_MYSQL "Disable Mysql" OFF)
    ENDIF(DROGON_FOUND_MYSQL)

    IF(DROGON_FOUND_SQLite3)
        OPTION(USE_SQLITE3 "Enable Sqlite3" ON)
    ELSE(DROGON_FOUND_SQLite3)
        OPTION(USE_SQLITE3 "Disable Sqlite3" OFF)
    ENDIF(DROGON_FOUND_SQLite3)
ENDIF(
    pg_FOUND
    OR DROGON_FOUND_MYSQL
    OR DROGON_FOUND_SQLite3
)

GET_FILENAME_COMPONENT(COMPILER_COMMAND ${CMAKE_CXX_COMPILER} NAME)
SET(COMPILER_ID ${CMAKE_CXX_COMPILER_ID})

IF(CMAKE_BUILD_TYPE)
    STRING(TOLOWER ${CMAKE_BUILD_TYPE} _type)
    IF(_type STREQUAL release)
        SET(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -std=c++")
    ELSEIF(_type STREQUAL debug)
        SET(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++")
    ELSE()
        SET(COMPILATION_FLAGS "-std=c++")
    ENDIF()
ELSE(CMAKE_BUILD_TYPE)
    SET(COMPILATION_FLAGS "-std=c++")
ENDIF(CMAKE_BUILD_TYPE)

LIST(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW "${CMAKE_INSTALL_PREFIX}/${INSTALL_INCLUDE_DIR}")
LIST(REMOVE_DUPLICATES INCLUDE_DIRS_FOR_DYNAMIC_VIEW)
SET(INS_STRING "")
FOREACH(loop_var ${INCLUDE_DIRS_FOR_DYNAMIC_VIEW})
    SET(INS_STRING "${INS_STRING} -I${loop_var}")
ENDFOREACH(loop_var)

SET(INCLUDING_DIRS ${INS_STRING})

CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/cmake/templates/config.h.in ${PROJECT_BINARY_DIR}/drogon/config.h @ONLY)

IF(BUILD_TESTING)
    MESSAGE(STATUS "Building tests")
    ENABLE_TESTING()
    ADD_SUBDIRECTORY(lib/tests)
    IF(pg_FOUND)
        ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl/test)
    ENDIF(pg_FOUND)
    IF(DROGON_FOUND_MYSQL)
        ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/src/mysql_impl/test)
    ENDIF(DROGON_FOUND_MYSQL)
    IF(DROGON_FOUND_SQLite3)
        ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/src/sqlite3_impl/test)
    ENDIF(DROGON_FOUND_SQLite3)
    ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/tests)
ENDIF(BUILD_TESTING)

# Installation

INSTALL(
    TARGETS ${PROJECT_NAME}
    EXPORT DrogonTargets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
)

INSTALL(FILES ${DROGON_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon)

SET(ORM_HEADERS
    orm_lib/inc/drogon/orm/ArrayParser.h
    orm_lib/inc/drogon/orm/BaseBuilder.h
    orm_lib/inc/drogon/orm/Criteria.h
    orm_lib/inc/drogon/orm/DbClient.h
    orm_lib/inc/drogon/orm/DbConfig.h
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
    orm_lib/inc/drogon/orm/RestfulController.h
)
INSTALL(FILES ${ORM_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/orm)

SET(NOSQL_HEADERS nosql_lib/redis/inc/drogon/nosql/RedisClient.h nosql_lib/redis/inc/drogon/nosql/RedisResult.h
                  nosql_lib/redis/inc/drogon/nosql/RedisSubscriber.h nosql_lib/redis/inc/drogon/nosql/RedisException.h
)
INSTALL(FILES ${NOSQL_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/nosql)

SET(DROGON_UTIL_HEADERS
    lib/inc/drogon/utils/coroutine.h lib/inc/drogon/utils/FunctionTraits.h lib/inc/drogon/utils/HttpConstraint.h
    lib/inc/drogon/utils/OStringStream.h lib/inc/drogon/utils/Utilities.h lib/inc/drogon/utils/monitoring.h
)
INSTALL(FILES ${DROGON_UTIL_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils)

SET(DROGON_MONITORING_HEADERS
    lib/inc/drogon/utils/monitoring/Counter.h
    lib/inc/drogon/utils/monitoring/Metric.h
    lib/inc/drogon/utils/monitoring/Registry.h
    lib/inc/drogon/utils/monitoring/Collector.h
    lib/inc/drogon/utils/monitoring/Sample.h
    lib/inc/drogon/utils/monitoring/Gauge.h
    lib/inc/drogon/utils/monitoring/Histogram.h
)

INSTALL(FILES ${DROGON_MONITORING_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils/monitoring)

SET(DROGON_PLUGIN_HEADERS
    lib/inc/drogon/plugins/Plugin.h
    lib/inc/drogon/plugins/Redirector.h
    lib/inc/drogon/plugins/SecureSSLRedirector.h
    lib/inc/drogon/plugins/AccessLogger.h
    lib/inc/drogon/plugins/RealIpResolver.h
    lib/inc/drogon/plugins/Hodor.h
    lib/inc/drogon/plugins/SlashRemover.h
    lib/inc/drogon/plugins/GlobalFilters.h
    lib/inc/drogon/plugins/PromExporter.h
)

INSTALL(FILES ${DROGON_PLUGIN_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/plugins)

TARGET_SOURCES(
    ${PROJECT_NAME}
    PRIVATE ${DROGON_SOURCES}
            ${private_headers}
            ${DROGON_HEADERS}
            ${ORM_HEADERS}
            ${DROGON_UTIL_HEADERS}
            ${DROGON_PLUGIN_HEADERS}
            ${NOSQL_HEADERS}
            ${DROGON_MONITORING_HEADERS}
)

SOURCE_GROUP(
    "Public API" FILES ${DROGON_HEADERS} ${ORM_HEADERS} ${DROGON_UTIL_HEADERS} ${DROGON_PLUGIN_HEADERS}
                       ${NOSQL_HEADERS} ${DROGON_MONITORING_HEADERS}
)
SOURCE_GROUP("Private Headers" FILES ${private_headers})

# Export the package for use from the build-tree (this registers the build-tree with a global cmake-registry)
# export(PACKAGE Drogon)

INCLUDE(CMakePackageConfigHelpers)
# ... for the install tree
CONFIGURE_PACKAGE_CONFIG_FILE(
    cmake/templates/DrogonConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake
    INSTALL_DESTINATION ${INSTALL_DROGON_CMAKE_DIR}
)

# version
WRITE_BASIC_PACKAGE_VERSION_FILE(
    ${CMAKE_CURRENT_BINARY_DIR}/DrogonConfigVersion.cmake
    VERSION ${DROGON_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the DrogonConfig.cmake and DrogonConfigVersion.cmake
INSTALL(
    FILES "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake"
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
    COMPONENT dev
)

# Install the export set for use with the install-tree
INSTALL(
    EXPORT DrogonTargets
    DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
    NAMESPACE Drogon::
    COMPONENT dev
)

# Doxygen documentation
FIND_PACKAGE(Doxygen OPTIONAL_COMPONENTS dot dia)
IF(DOXYGEN_FOUND)
    SET(DOXYGEN_PROJECT_BRIEF "C++14/17-based HTTP application framework")
    SET(DOXYGEN_OUTPUT_DIRECTORY docs/${PROJECT_NAME})
    SET(DOXYGEN_GENERATE_LATEX NO)
    SET(DOXYGEN_BUILTIN_STL_SUPPORT YES)
    SET(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
    SET(DOXYGEN_STRIP_FROM_INC_PATH ${PROJECT_SOURCE_DIR}/lib/inc ${PROJECT_SOURCE_DIR}/orm_lib/inc
                                    ${CMAKE_CURRENT_BINARY_DIR}/exports
    )
    SET(DOXYGEN_EXAMPLE_PATTERNS *)
    IF(WIN32)
        SET(DOXYGEN_PREDEFINED _WIN32)
    ENDIF(WIN32)
    DOXYGEN_ADD_DOCS(
        doc_${PROJECT_NAME}
        README.md
        README.zh-CN.md
        README.zh-TW.md
        ChangeLog.md
        CONTRIBUTING.md
        ${DROGON_HEADERS}
        ${DROGON_UTIL_HEADERS}
        ${DROGON_PLUGIN_HEADERS}
        ${ORM_HEADERS}
        COMMENT "Generate documentation"
    )
    IF(NOT TARGET doc)
        ADD_CUSTOM_TARGET(doc)
    ENDIF()
    ADD_DEPENDENCIES(doc doc_${PROJECT_NAME})
    IF(BUILD_DOC)
        ADD_DEPENDENCIES(${PROJECT_NAME} doc_${PROJECT_NAME})
        # Don't install twice, so limit to Debug (assume developer)
        INSTALL(
            DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/${PROJECT_NAME}
            TYPE DOC
            CONFIGURATIONS Debug
        )
    ENDIF(BUILD_DOC)
ENDIF(DOXYGEN_FOUND)

INCLUDE(cmake/Packages.cmake)
