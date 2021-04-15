cmake_minimum_required(VERSION 3.5)

##### PLATFORM deps #####
set(HUNTER_ROOT "{{HUNTER_ROOT}}")
include(HunterGate)
HunterGate(
    URL "unused" 
    SHA1 "unused" 
)

project(drogon)

message(STATUS "compiler: " ${CMAKE_CXX_COMPILER_ID})

{{#platform_deps}}
  hunter_add_package({{pkg_name}} COMPONENTS {{#components}}{{component}} {{/components}})
{{/platform_deps}}

# If your cross compile is failing, you should set
# CMAKE_SYSTEM_NAME in your toolchain file
if (NOT CMAKE_CROSSCOMPILING)
    set(BUILD_PROGRAMS ON)
else ()
    set(BUILD_PROGRAMS OFF)
endif ()

option(BUILD_CTL "Build drogon_ctl" ${BUILD_PROGRAMS})
option(BUILD_EXAMPLES "Build examples" ${BUILD_PROGRAMS})
option(BUILD_ORM "Build orm" ON)
option(COZ_PROFILING "Use coz for profiling" OFF)
option(BUILD_DROGON_SHARED "Build drogon as a shared lib" OFF)

include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_POSTGRESQL "Build with postgresql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(LIBPQ_BATCH_MODE "Use batch mode for libpq" ON "BUILD_POSTGRESQL" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_MYSQL "Build with mysql support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_SQLITE "Build with sqlite3 support" ON "BUILD_ORM" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_REDIS "Build with redis support" ON "BUILD_ORM" OFF)

set(DROGON_MAJOR_VERSION 1)
set(DROGON_MINOR_VERSION 4)
set(DROGON_PATCH_VERSION 1)
set(DROGON_VERSION
    ${DROGON_MAJOR_VERSION}.${DROGON_MINOR_VERSION}.${DROGON_PATCH_VERSION})
set(DROGON_VERSION_STRING "${DROGON_VERSION}")

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
set(DEF_INSTALL_DROGON_CMAKE_DIR lib/cmake/nxxm_drogon)
set(INSTALL_DROGON_CMAKE_DIR ${DEF_INSTALL_DROGON_CMAKE_DIR}
    CACHE PATH "Installation directory for cmake files")

if (BUILD_DROGON_SHARED)
    set(BUILD_TRANTOR_SHARED TRUE)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    # set(BUILD_EXAMPLES FALSE)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
        "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" isSystemDir)
    if ("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")
    endif ("${isSystemDir}" STREQUAL "-1")
    add_library(${PROJECT_NAME} SHARED)
else (BUILD_DROGON_SHARED)
    add_library(${PROJECT_NAME} STATIC)
endif (BUILD_DROGON_SHARED)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/DrogonUtilities.cmake)
include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
if (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(DROGON_CXX_STANDARD 20)
elseif (HAS_ANY AND HAS_STRING_VIEW)
    set(DROGON_CXX_STANDARD 17)
else ()
    set(DROGON_CXX_STANDARD 14)
endif ()

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../lib/inc>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/inc>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../trantor>
    $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>)

if (WIN32)
    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../third_party/mman-win32>)
endif (WIN32)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../trantor trantor)

target_link_libraries(${PROJECT_NAME} PUBLIC trantor)

if (NOT WIN32)
    if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
        target_link_libraries(${PROJECT_NAME} PRIVATE dl)
    endif (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
else (NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)
endif (NOT WIN32)

if (DROGON_CXX_STANDARD EQUAL 14)
    # With C++14, use boost to support any and string_view
    message(STATUS "use c++14")
    find_package(Boost 1.61.0 REQUIRED)
    message(STATUS "boost include dir:" ${Boost_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::boost)
    list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${Boost_INCLUDE_DIR})
elseif (DROGON_CXX_STANDARD EQUAL 17)
    message(STATUS "use c++17")
else ()
    message(STATUS "use c++20")
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/)

# jsoncpp
find_package(open-source-parsers_jsoncpp REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC open-source-parsers_jsoncpp::jsoncpp)
list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${JSONCPP_INCLUDE_DIRS})

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
    AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD"
    AND NOT WIN32)
    find_package(nxxm_libuuid CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE nxxm_libuuid::libuuid)

    try_compile(normal_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${CMAKE_CURRENT_LIST_DIR}/../../cmake/tests/normal_uuid_lib_test.cc
        LINK_LIBRARIES nxxm_libuuid::libuuid)
    try_compile(ossp_uuid ${CMAKE_BINARY_DIR}/cmaketest
        ${CMAKE_CURRENT_LIST_DIR}/../../cmake/tests/ossp_uuid_lib_test.cc
        LINK_LIBRARIES nxxm_libuuid::libuuid)
    if (normal_uuid)
        add_definitions(-DUSE_OSSP_UUID=0)
    elseif (ossp_uuid)
        add_definitions(-DUSE_OSSP_UUID=1)
    else ()
        message(FATAL_ERROR "uuid lib error")
    endif ()
endif (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
    AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD"
    AND NOT WIN32)

find_package(Brotli)
if (Brotli_FOUND)
    message(STATUS "Brotli found")
    add_definitions(-DUSE_BROTLI)
    target_link_libraries(${PROJECT_NAME} PRIVATE Brotli_lib)
endif (Brotli_FOUND)

set(DROGON_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/AOPAdvice.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/CacheFile.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/ConfigLoader.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/Cookie.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/DrClassMap.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/DrTemplateBase.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/FiltersFunction.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpAppFrameworkImpl.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpBinder.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpClientImpl.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpControllersRouter.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpFileImpl.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpFileUploadRequest.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpRequestImpl.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpRequestParser.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpResponseImpl.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpResponseParser.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpServer.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpSimpleControllersRouter.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpUtils.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/HttpViewData.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/IntranetIpFilter.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/ListenerManager.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/LocalHostFilter.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/MultiPart.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/NotFound.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/PluginsManager.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/SecureSSLRedirector.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/SessionManager.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/StaticFileRouter.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/Utilities.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/WebSocketClientImpl.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/WebSocketConnectionImpl.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/WebsocketControllersRouter.cc)

if (NOT WIN32)
    set(DROGON_SOURCES ${DROGON_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/SharedLibManager.cc)
else (NOT WIN32)
    set(DROGON_SOURCES ${DROGON_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/../../third_party/mman-win32/mman.c)
endif (NOT WIN32)

if (BUILD_POSTGRESQL)
    # find postgres
    find_package(pg)
    if (pg_FOUND)
        message(STATUS "libpq inc path:" ${PG_INCLUDE_DIRS})
        message(STATUS "libpq lib:" ${PG_LIBRARIES})
        target_link_libraries(${PROJECT_NAME} PRIVATE pg_lib)
        set(DROGON_SOURCES ${DROGON_SOURCES}
            orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc)
        if (LIBPQ_BATCH_MODE)
            try_compile(libpq_supports_batch ${CMAKE_BINARY_DIR}/cmaketest
                ${CMAKE_CURRENT_LIST_DIR}/../../cmake/tests/test_libpq_batch_mode.cc
                LINK_LIBRARIES ${PostgreSQL_LIBRARIES}
                CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PostgreSQL_INCLUDE_DIR}")
        endif (LIBPQ_BATCH_MODE)
        if (libpq_supports_batch)
            message(STATUS "The libpq supports batch mode")
            option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" ON)
            set(DROGON_SOURCES ${DROGON_SOURCES}
                orm_lib/src/postgresql_impl/PgBatchConnection.cc)
        else (libpq_supports_batch)
            option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" OFF)
            set(DROGON_SOURCES ${DROGON_SOURCES}
                orm_lib/src/postgresql_impl/PgConnection.cc)
        endif (libpq_supports_batch)
    endif (pg_FOUND)
endif (BUILD_POSTGRESQL)

if (BUILD_MYSQL)
    # Find mysql, only mariadb client liberary is supported
    find_package(MySQL)
    if (MySQL_FOUND)
        message(STATUS "Ok! We find the mariadb!")
        target_link_libraries(${PROJECT_NAME} PRIVATE MySQL_lib)
        set(DROGON_SOURCES ${DROGON_SOURCES}
            orm_lib/src/mysql_impl/MysqlConnection.cc
            orm_lib/src/mysql_impl/MysqlResultImpl.cc)
    endif (MySQL_FOUND)
endif (BUILD_MYSQL)

if (BUILD_SQLITE)
    # Find sqlite3.
    find_package(SQLite3)
    if (SQLite3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE SQLite3_lib)
        set(DROGON_SOURCES ${DROGON_SOURCES}
           ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
           ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc)
    endif (SQLite3_FOUND)
endif (BUILD_SQLITE)

if (BUILD_REDIS)
    find_package(Hiredis)
    if (Hiredis_FOUND)
        add_definitions(-DUSE_REDIS)
        target_link_libraries(${PROJECT_NAME} PRIVATE Hiredis_lib)
        set(DROGON_SOURCES
            ${DROGON_SOURCES}
            ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/src/RedisClientImpl.cc
            ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/src/RedisConnection.cc
            ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/src/RedisResult.cc
            ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/src/RedisClientLockFree.cc
            ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/src/RedisClientManager.cc
            ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/src/RedisTransactionImpl.cc)

        if (BUILD_TESTING)
            add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/tests redis_tests)
        endif (BUILD_TESTING)
    else ()
        set(DROGON_SOURCES ${DROGON_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/RedisClientManagerSkipped.cc)
    endif (Hiredis_FOUND)
else ()
    set(DROGON_SOURCES ${DROGON_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/RedisClientManagerSkipped.cc)
endif (BUILD_REDIS)

find_package(ZLIB CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)

find_package(OpenSSL)
if (OpenSSL_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else (OpenSSL_FOUND)
    set(DROGON_SOURCES ${DROGON_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/ssl_funcs/Md5.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/ssl_funcs/Sha1.cc)
endif (OpenSSL_FOUND)

execute_process(COMMAND "git" rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_SHA1
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../cmake/templates/version.h.in"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/version.h" @ONLY)

if (DROGON_CXX_STANDARD EQUAL 20)
    option(USE_COROUTINE "Enable C++20 coroutine support" ON)
else (DROGON_CXX_STANDARD EQUAL 20)
    option(USE_COROUTINE "Enable C++20 coroutine support" OFF)
endif (DROGON_CXX_STANDARD EQUAL 20)

if (BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../examples examples)
endif (BUILD_EXAMPLES)

if (BUILD_CTL)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../drogon_ctl drogon_ctl)
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

if (pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
    set(DROGON_SOURCES
        ${DROGON_SOURCES}
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/ArrayParser.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/Criteria.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/DbClient.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/DbClientImpl.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/DbClientLockFree.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/DbClientManager.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/DbConnection.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/Exception.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/Field.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/Result.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/Row.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/SqlBinder.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/TransactionImpl.cc
        ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/RestfulController.cc)
else (pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
    set(DROGON_SOURCES ${DROGON_SOURCES} ${CMAKE_CURRENT_LIST_DIR}/../../lib/src/DbClientManagerSkipped.cc)
endif (pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)

target_sources(${PROJECT_NAME} PRIVATE ${DROGON_SOURCES})

set_target_properties(${PROJECT_NAME}
    PROPERTIES CXX_STANDARD ${DROGON_CXX_STANDARD})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME Drogon)

if (pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
    if (pg_FOUND)
        option(USE_POSTGRESQL "Enable PostgreSQL" ON)
    else (pg_FOUND)
        option(USE_POSTGRESQL "Disable PostgreSQL" OFF)
    endif (pg_FOUND)

    if (MySQL_FOUND)
        option(USE_MYSQL "Enable Mysql" ON)
    else (MySQL_FOUND)
        option(USE_MYSQL "DisableMysql" OFF)
    endif (MySQL_FOUND)

    if (SQLite3_FOUND)
        option(USE_SQLITE3 "Enable Sqlite3" ON)
    else (SQLite3_FOUND)
        option(USE_SQLITE3 "Disable Sqlite3" OFF)
    endif (SQLite3_FOUND)
endif (pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)

set(COMPILER_COMMAND ${CMAKE_CXX_COMPILER})
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

configure_file(${CMAKE_CURRENT_LIST_DIR}/../../cmake/templates/config.h.in
    ${PROJECT_BINARY_DIR}/drogon/config.h @ONLY)

if (BUILD_TESTING)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/tests lib_tests)
    if (pg_FOUND)
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/postgresql_impl/test)
    endif (pg_FOUND)
    if (MySQL_FOUND)
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/mysql_impl/test)
    endif (MySQL_FOUND)
    if (SQLite3_FOUND)
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/src/sqlite3_impl/test)
    endif (SQLite3_FOUND)
    if (pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/tests)
    endif (pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
    find_package(GTest)
    if (GTest_FOUND)
        message(STATUS "gtest found")
        enable_testing()
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../unittest unittest)
    endif (GTest_FOUND)
endif (BUILD_TESTING)

# Installation

install(TARGETS ${PROJECT_NAME}
    EXPORT DrogonTargets
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)

set(DROGON_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/Attribute.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/CacheMap.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/Cookie.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/DrClassMap.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/DrObject.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/DrTemplate.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/DrTemplateBase.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpAppFramework.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpBinder.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpClient.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpController.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpFilter.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpRequest.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpResponse.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpSimpleController.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpTypes.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/HttpViewData.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/IntranetIpFilter.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/IOThreadStorage.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/LocalHostFilter.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/MultiPart.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/NotFound.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/Session.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/UploadFile.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/WebSocketClient.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/WebSocketConnection.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/WebSocketController.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/drogon.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/version.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/drogon_callbacks.h
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/PubSubService.h)
install(FILES ${DROGON_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon)

set(ORM_HEADERS
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/ArrayParser.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/Criteria.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/DbClient.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/DbTypes.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/Exception.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/Field.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/FunctionTraits.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/Mapper.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/Result.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/ResultIterator.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/Row.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/RowIterator.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/SqlBinder.h
   ${CMAKE_CURRENT_LIST_DIR}/../../orm_lib/inc/drogon/orm/RestfulController.h)
install(FILES ${ORM_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/orm)

set(NOSQL_HEADERS 
   ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/inc/drogon/nosql/RedisClient.h
   ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/inc/drogon/nosql/RedisResult.h
   ${CMAKE_CURRENT_LIST_DIR}/../../nosql_lib/redis/inc/drogon/nosql/RedisException.h)
install(FILES ${NOSQL_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/nosql)

set(DROGON_UTIL_HEADERS
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/FunctionTraits.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/Utilities.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/any.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/string_view.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/optional.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/coroutine.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/HttpConstraint.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/utils/OStringStream.h)
install(FILES ${DROGON_UTIL_HEADERS}
    DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils)

set(DROGON_PLUGIN_HEADERS 
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/plugins/Plugin.h
   ${CMAKE_CURRENT_LIST_DIR}/../../lib/inc/drogon/plugins/SecureSSLRedirector.h)
install(FILES ${DROGON_PLUGIN_HEADERS}
    DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/plugins)

source_group("Public API"
    FILES
    ${DROGON_HEADERS}
    ${ORM_HEADERS}
    ${DROGON_UTIL_HEADERS}
    ${DROGON_PLUGIN_HEADERS}
    ${NOSQL_HEADERS})

# Export the package for use from the build-tree (this registers the build-tree
# with a global cmake-registry) export(PACKAGE Drogon)

include(CMakePackageConfigHelpers)
# ... for the install tree
configure_package_config_file(
   ${CMAKE_CURRENT_LIST_DIR}/../../cmake/templates/DrogonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/nxxm_drogonConfig.cmake
    INSTALL_DESTINATION
    ${INSTALL_DROGON_CMAKE_DIR})

# version
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/nxxm_drogonConfigVersion.cmake
    VERSION ${DROGON_VERSION}
    COMPATIBILITY SameMajorVersion)

# Install the DrogonConfig.cmake and DrogonConfigVersion.cmake
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/nxxm_drogonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/nxxm_drogonConfigVersion.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/FindUUID.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/FindJsoncpp.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/FindSQLite3.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/FindMySQL.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/Findpg.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/FindBrotli.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/Findcoz-profiler.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake_modules/FindHiredis.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake/DrogonUtilities.cmake"
    DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
    COMPONENT dev)

# Install the export set for use with the install-tree
install(EXPORT DrogonTargets
    DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
    NAMESPACE Drogon::
    COMPONENT dev)
