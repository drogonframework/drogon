CMAKE_MINIMUM_REQUIRED (VERSION 3.2)

PROJECT (DROGON CXX)
MESSAGE (STATUS "os:" ${CMAKE_SYSTEM_NAME})

include(CheckIncludeFileCXX)

CHECK_INCLUDE_FILE_CXX(any HAS_ANY)
CHECK_INCLUDE_FILE_CXX(string_view HAS_STRING_VIEW)
IF(HAS_ANY AND HAS_STRING_VIEW)
    SET(DROGON_CXX_STANDARD 17)
ELSE()
    SET(DROGON_CXX_STANDARD 14)
ENDIF()

IF(DROGON_CXX_STANDARD LESS 17)
#With C++14, use boost to support any and string_view
    MESSAGE(STATUS "use c++14")
    FIND_PACKAGE(Boost 1.61.0 REQUIRED)
    IF(Boost_FOUND)
        INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
    ENDIF()
ELSE()
    MESSAGE(STATUS "use c++17")
ENDIF()

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/trantor 
                    ${PROJECT_SOURCE_DIR}/lib/inc 
                    ${PROJECT_SOURCE_DIR}/orm_lib/inc)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

#jsoncpp
FIND_PACKAGE (Jsoncpp REQUIRED)
INCLUDE_DIRECTORIES(${JSONCPP_INCLUDE_DIRS})
LINK_LIBRARIES(${JSONCPP_LIBRARIES})

MESSAGE(STATUS "jsoncpp inc path:" ${JSONCPP_INCLUDE_DIRS})

#MESSAGE(STATUS ${JSONCPP_LIBRARIES})
IF(NOT EXISTS ${JSONCPP_INCLUDE_DIRS}/json/version.h)
    MESSAGE(FATAL_ERROR "Error: jsoncpp lib is too old.....stop")
ENDIF()

EXEC_PROGRAM(cat ARGS "${JSONCPP_INCLUDE_DIRS}/json/version.h |grep JSONCPP_VERSION_STRING|sed s'/.*define/define/'|awk '{printf $3}'|sed s'/\"//g'" OUTPUT_VARIABLE jsoncpp_ver)
MESSAGE(STATUS "jsoncpp verson:" ${jsoncpp_ver})
IF(jsoncpp_ver LESS 1.7)
    MESSAGE(FATAL_ERROR "jsoncpp lib is too old,please get new version from https://github.com/open-source-parsers/jsoncpp")
ENDIF()

FIND_PACKAGE (UUID REQUIRED)
INCLUDE_DIRECTORIES(${UUID_INCLUDE_DIR})
LINK_LIBRARIES(${UUID_LIBRARIES})

FIND_PACKAGE(ZLIB REQUIRED)
INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
LINK_LIBRARIES(${ZLIB_LIBRARIES})

SET(drogon_sources  lib/src/AOPAdvice.cc
                    lib/src/CacheFile.cc
                    lib/src/ConfigLoader.cc
                    lib/src/Cookie.cc
                    lib/src/DrClassMap.cc
                    lib/src/DrTemplateBase.cc
                    lib/src/FiltersFunction.cc
                    lib/src/HttpAppFrameworkImpl.cc
                    lib/src/HttpClientImpl.cc
                    lib/src/HttpControllersRouter.cc
                    lib/src/HttpFileUploadRequest.cc
                    lib/src/HttpRequestImpl.cc
                    lib/src/HttpRequestParser.cc
                    lib/src/HttpResponseImpl.cc
                    lib/src/HttpResponseParser.cc
                    lib/src/HttpServer.cc
                    lib/src/HttpSimpleControllersRouter.cc
                    lib/src/HttpUtils.cc
                    lib/src/HttpViewBase.cc
                    lib/src/HttpViewData.cc
                    lib/src/IntranetIpFilter.cc
                    lib/src/ListenerManager.cc
                    lib/src/LocalHostFilter.cc
                    lib/src/MultiPart.cc
                    lib/src/NotFound.cc
                    lib/src/PluginsManager.cc
                    lib/src/SessionManager.cc
                    lib/src/SharedLibManager.cc
                    lib/src/StaticFileRouter.cc
                    lib/src/Utilities.cc
                    lib/src/WebSocketClientImpl.cc
                    lib/src/WebSocketConnectionImpl.cc
                    lib/src/WebsocketControllersRouter.cc)

FIND_PACKAGE (OpenSSL)
IF(OpenSSL_FOUND)
    INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
    LINK_LIBRARIES(${OPENSSL_LIBRARIES})
    MESSAGE(STATUS "openssl inc path:" ${OPENSSL_INCLUDE_DIR})
ELSE()
    SET(drogon_sources ${drogon_sources}  lib/src/ssl_funcs/Md5.cc
                                          lib/src/ssl_funcs/Sha1.cc)
ENDIF()




IF(NOT BUILD_ORM)
   SET(BUILD_ORM TRUE CACHE BOOL INTERNAL)
ENDIF()


IF(BUILD_ORM)

    #find postgres
    FIND_PACKAGE(PostgreSQL)
    IF(PostgreSQL_FOUND)
        INCLUDE_DIRECTORIES(${PostgreSQL_INCLUDE_DIR})
        MESSAGE(STATUS "libpq inc path:" ${PostgreSQL_INCLUDE_DIR})
        LINK_LIBRARIES(${PostgreSQL_LIBRARIES})
        SET(drogon_sources ${drogon_sources}  
            orm_lib/src/postgresql_impl/PgConnection.cc
            orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc)
    ENDIF()

    #Find mysql, only mariadb client liberary is supported
    FIND_PACKAGE(MySQL)
    IF(MYSQL_FOUND)
        MESSAGE(STATUS "inc:" ${MYSQL_INCLUDE_DIR})
        MESSAGE(STATUS "libs:" ${MYSQL_CLIENT_LIBS})
        MESSAGE(STATUS "version:" ${MYSQL_VERSION_STRING})
        IF(MYSQL_VERSION_STRING STREQUAL "")
            SET(MYSQL_FOUND false)
            MESSAGE(STATUS "The mysql in your system is not the mariadb, so we can't use it in drogon")
        ELSE()
            MESSAGE(STATUS "Ok! We find the mariadb!")
            INCLUDE_DIRECTORIES(${MYSQL_INCLUDE_DIR})
            LINK_LIBRARIES(${MYSQL_CLIENT_LIBS})
            SET(drogon_sources ${drogon_sources}  
                orm_lib/src/mysql_impl/MysqlConnection.cc
                orm_lib/src/mysql_impl/MysqlResultImpl.cc)
        ENDIF()
    ENDIF()

    #Find sqlite3.
    FIND_PACKAGE (SQLite3)
    if (SQLITE3_FOUND)
        MESSAGE(STATUS "SQLite3 inc:" ${SQLITE3_INCLUDE_DIRS})
        INCLUDE_DIRECTORIES(${SQLITE3_INCLUDE_DIRS})
        LINK_LIBRARIES(${SQLITE3_LIBRARIES})
        SET(drogon_sources ${drogon_sources}  
            orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
            orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc)
    ENDIF()
ENDIF()

ADD_SUBDIRECTORY(trantor)

add_custom_target(makeVersion)
add_custom_command(TARGET makeVersion
        COMMAND ${PROJECT_SOURCE_DIR}/get_version.sh
        ARGS ${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM )

IF(NOT BUILD_EXAMPLES)
  SET(BUILD_EXAMPLES TRUE CACHE BOOL INTERNAL)
ENDIF()

IF(BUILD_EXAMPLES)
    ADD_SUBDIRECTORY(examples)
ENDIF()

IF(NOT BUILD_CTL)
  SET(BUILD_CTL TRUE CACHE BOOL INTERNAL)
ENDIF()

IF(BUILD_CTL)
    ADD_SUBDIRECTORY(drogon_ctl)
ENDIF()

IF(PostgreSQL_FOUND OR MYSQL_FOUND OR SQLITE3_FOUND)
    SET(drogon_sources ${drogon_sources}  
        orm_lib/src/ArrayParser.cc
        orm_lib/src/Criteria.cc
        orm_lib/src/DbClient.cc
        orm_lib/src/DbClientImpl.cc
        orm_lib/src/DbClientLockFree.cc
        orm_lib/src/DbClientManager.cc
        orm_lib/src/Exception.cc
        orm_lib/src/Field.cc
        orm_lib/src/Result.cc
        orm_lib/src/Row.cc
        orm_lib/src/SqlBinder.cc
        orm_lib/src/TransactionImpl.cc)
ELSE()
    SET(drogon_sources ${drogon_sources} lib/src/DbClientManagerSkipped.cc)
ENDIF()

ADD_LIBRARY(drogon ${drogon_sources})

SET_PROPERTY(TARGET drogon PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
SET_PROPERTY(TARGET drogon PROPERTY CXX_STANDARD_REQUIRED ON)
SET_PROPERTY(TARGET drogon PROPERTY CXX_EXTENSIONS OFF)

ADD_DEPENDENCIES(drogon trantor makeVersion)

IF(PostgreSQL_FOUND OR MYSQL_FOUND OR SQLITE3_FOUND)
    IF(PostgreSQL_FOUND)
        OPTION(USE_POSTGRESQL "Enable PostgreSQL" ON)
    ELSE()
        OPTION(USE_POSTGRESQL "Disable PostgreSQL" OFF)
    ENDIF()

    IF(MYSQL_FOUND)
        OPTION(USE_MYSQL "Enable Mysql" ON)
    ELSE()
        OPTION(USE_MYSQL "DisableMysql" OFF)
    ENDIF()

    IF(SQLITE3_FOUND)
        OPTION(USE_SQLITE3 "Enable Sqlite3" ON)
    ELSE()
        OPTION(USE_SQLITE3 "Disable Sqlite3" OFF)
    ENDIF()
ENDIF()

SET(COMPILER_COMMAND ${CMAKE_CXX_COMPILER})
SET(COMPILER_ID ${CMAKE_CXX_COMPILER_ID})


IF(CMAKE_BUILD_TYPE)
    string(TOLOWER ${CMAKE_BUILD_TYPE} _type)
    IF(_type STREQUAL release)
        SET(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -std=c++")
    ELSEIF(_type STREQUAL debug)
        SET(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++")
    ELSE()
        SET(COMPILATION_FLAGS "-std=c++")
    ENDIF()
ELSE()
    SET(COMPILATION_FLAGS "-std=c++")
ENDIF()

GET_TARGET_PROPERTY(INS drogon INCLUDE_DIRECTORIES)
SET(TMP_INS "")
SET(INS_STRING "")
FOREACH(loop_var ${INS})
    IF(TMP_INS MATCHES ";${loop_var};")
    ELSE()
        SET(TMP_INS ";${loop_var};${TMP_INS}")
        SET(INS_STRING "${INS_STRING} -I${loop_var}")
    ENDIF()
ENDFOREACH(loop_var)

SET(INCLUDING_DIRS ${INS_STRING})

configure_file(${PROJECT_SOURCE_DIR}/lib/inc/drogon/config.h.in 
               ${PROJECT_SOURCE_DIR}/lib/inc/drogon/config.h @ONLY)

if (MAKETEST STREQUAL YES)
    ADD_SUBDIRECTORY(lib/tests)
    IF(PostgreSQL_FOUND)
        ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl/test)
        ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/tests)
    ENDIF()
    IF(MYSQL_FOUND)
        ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/src/mysql_impl/test)
    ENDIF()
    IF(SQLITE3_FOUND)
        ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/orm_lib/src/sqlite3_impl/test)
    ENDIF()
endif ()

#Installation
INSTALL(TARGETS drogon DESTINATION lib)

SET(drogon_headers 
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
    lib/inc/drogon/HttpViewBase.h
    lib/inc/drogon/HttpViewData.h
    lib/inc/drogon/IntranetIpFilter.h
    lib/inc/drogon/LocalHostFilter.h
    lib/inc/drogon/MultiPart.h
    lib/inc/drogon/NotFound.h
    lib/inc/drogon/Session.h
    lib/inc/drogon/UploadFile.h
    lib/inc/drogon/WebSocketClient.h
    lib/inc/drogon/WebSocketConnection.h
    lib/inc/drogon/WebSocketController.h
    lib/inc/drogon/drogon.h
    lib/inc/drogon/version.h)
INSTALL(FILES ${drogon_headers} DESTINATION include/drogon)

SET(orm_headers 
    orm_lib/inc/drogon/orm/ArrayParser.h
    orm_lib/inc/drogon/orm/Criteria.h
    orm_lib/inc/drogon/orm/DbClient.h
    orm_lib/inc/drogon/orm/Exception.h
    orm_lib/inc/drogon/orm/Field.h
    orm_lib/inc/drogon/orm/FunctionTraits.h
    orm_lib/inc/drogon/orm/Mapper.h
    orm_lib/inc/drogon/orm/Result.h
    orm_lib/inc/drogon/orm/ResultIterator.h
    orm_lib/inc/drogon/orm/Row.h
    orm_lib/inc/drogon/orm/RowIterator.h
    orm_lib/inc/drogon/orm/SqlBinder.h)
INSTALL(FILES ${orm_headers} DESTINATION include/drogon/orm)

SET(drogon_util_headers 
    lib/inc/drogon/utils/ClassTraits.h
    lib/inc/drogon/utils/FunctionTraits.h
    lib/inc/drogon/utils/Utilities.h
    lib/inc/drogon/utils/any.h
    lib/inc/drogon/utils/string_view.h
    lib/inc/drogon/utils/HttpConstraint.h)
INSTALL(FILES ${drogon_util_headers}
        DESTINATION include/drogon/utils)

SET(drogon_plugin_headers lib/inc/drogon/plugins/Plugin.h)
INSTALL(FILES ${drogon_plugin_headers}
        DESTINATION include/drogon/plugins)

SOURCE_GROUP( "Public API" FILES ${drogon_headers} 
                                 ${orm_headers}
                                 ${drogon_util_headers}
                                 ${drogon_plugin_headers})
