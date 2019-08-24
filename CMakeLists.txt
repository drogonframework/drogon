cmake_minimum_required (VERSION 3.2)

project (DROGON CXX)

set(DROGON_MAJOR_VERSION 1)
set(DROGON_MINOR_VERSION 0)
set(DROGON_PATCH_VERSION 0)
set(DROGON_VERSION
  ${DROGON_MAJOR_VERSION}.${DROGON_MINOR_VERSION}.${DROGON_PATCH_VERSION})
set(PRE_RELEASE_STRING "beta6")

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH
  "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_DROGON_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_DROGON_CMAKE_DIR lib/cmake/Drogon)
endif()
set(INSTALL_DROGON_CMAKE_DIR ${DEF_INSTALL_DROGON_CMAKE_DIR} CACHE PATH
  "Installation directory for cmake files")
# Make relative paths absolute (needed later on)
foreach(p LIB BIN INCLUDE DROGON_CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

include(CheckIncludeFileCXX)

CHECK_INCLUDE_FILE_CXX(any HAS_ANY)
CHECK_INCLUDE_FILE_CXX(string_view HAS_STRING_VIEW)
if(HAS_ANY AND HAS_STRING_VIEW)
    set(DROGON_CXX_STANDARD 17)
else()
    set(DROGON_CXX_STANDARD 14)
endif()

add_subdirectory(trantor)
set(Trantor_DIR ${PROJECT_BINARY_DIR}/trantor)
find_package(Trantor CONFIG REQUIRED NO_DEFAULT_PATH)
message(STATUS "trantor inc:" ${TRANTOR_INCLUDE_DIRS})
include_directories(${TRANTOR_INCLUDE_DIRS})
link_libraries(${TRANTOR_LIBRARIES})

if(DROGON_CXX_STANDARD LESS 17)
#With C++14, use boost to support any and string_view
    message(STATUS "use c++14")
    find_package(Boost 1.61.0 REQUIRED)
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(STATUS "use c++17")
endif()

include_directories(${PROJECT_SOURCE_DIR}/lib/inc 
                    ${PROJECT_SOURCE_DIR}/orm_lib/inc
                    ${PROJECT_BINARY_DIR})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

#jsoncpp
find_package (Jsoncpp REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIRS})
link_libraries(${JSONCPP_LIBRARIES})
message(STATUS "jsoncpp inc path:" ${JSONCPP_INCLUDE_DIRS})

#message(STATUS ${JSONCPP_LIBRARIES})
if(NOT EXISTS ${JSONCPP_INCLUDE_DIRS}/json/version.h)
    message(FATAL_ERROR "Error: jsoncpp lib is too old.....stop")
endif()

exec_program(cat ARGS "${JSONCPP_INCLUDE_DIRS}/json/version.h |grep JSONCPP_VERSION_STRING|sed s'/.*define/define/'|awk '{printf $3}'|sed s'/\"//g'" OUTPUT_VARIABLE jsoncpp_ver)
message(STATUS "jsoncpp verson:" ${jsoncpp_ver})
if(jsoncpp_ver LESS 1.7)
    message(FATAL_ERROR "jsoncpp lib is too old,please get new version from https://github.com/open-source-parsers/jsoncpp")
endif()

find_package (UUID REQUIRED)
include_directories(${UUID_INCLUDE_DIR})
link_libraries(${UUID_LIBRARIES})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
link_libraries(${ZLIB_LIBRARIES})

set(DROGON_SOURCES  lib/src/AOPAdvice.cc
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

find_package (OpenSSL)
if(OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    link_libraries(${OPENSSL_LIBRARIES})
    message(STATUS "openssl inc path:" ${OPENSSL_INCLUDE_DIR})
else()
    set(DROGON_SOURCES ${DROGON_SOURCES}  lib/src/ssl_funcs/Md5.cc
                                          lib/src/ssl_funcs/Sha1.cc)
endif()




if(NOT BUILD_ORM)
   set(BUILD_ORM TRUE CACHE BOOL INTERNAL)
endif()

if(NOT LIBPQ_BATCH_MODE)
   set(LIBPQ_BATCH_MODE TRUE CACHE BOOL INTERNAL)
endif()

if(BUILD_ORM)

    #find postgres
    find_package(PostgreSQL)
    if(PostgreSQL_FOUND)
        message(STATUS "libpq inc path:" ${PostgreSQL_INCLUDE_DIR})
        message(STATUS "libpq lib:" ${PostgreSQL_LIBRARIES})
        include_directories(${PostgreSQL_INCLUDE_DIR})
        link_libraries(${PostgreSQL_LIBRARIES})
        set(DROGON_SOURCES ${DROGON_SOURCES}
            orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc)
        if(LIBPQ_BATCH_MODE)
        try_compile(libpq_supports_batch
                    ${CMAKE_BINARY_DIR}/cmaketest 
                    ${PROJECT_SOURCE_DIR}/cmake/tests/test_libpq_batch_mode.cc
                    LINK_LIBRARIES ${PostgreSQL_LIBRARIES}
                    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PostgreSQL_INCLUDE_DIR}")
        endif()
        if(libpq_supports_batch)
            message(STATUS "The libpq supports batch mode")
            option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" ON)
            set(DROGON_SOURCES ${DROGON_SOURCES}  
            orm_lib/src/postgresql_impl/PgBatchConnection.cc)
        else()
            option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" OFF)
            set(DROGON_SOURCES ${DROGON_SOURCES}  
            orm_lib/src/postgresql_impl/PgConnection.cc)
        endif()
    endif()

    #Find mysql, only mariadb client liberary is supported
    find_package(MySQL)
    if(MYSQL_FOUND)
        message(STATUS "inc:" ${MYSQL_INCLUDE_DIR})
        message(STATUS "libs:" ${MYSQL_CLIENT_LIBS})
        message(STATUS "version:" ${MYSQL_VERSION_STRING})
        if(MYSQL_VERSION_STRING STREQUAL "")
            set(MYSQL_FOUND false)
            message(STATUS "The mysql in your system is not the mariadb, so we can't use it in drogon")
        else()
            message(STATUS "Ok! We find the mariadb!")
            include_directories(${MYSQL_INCLUDE_DIR})
            link_libraries(${MYSQL_CLIENT_LIBS})
            set(DROGON_SOURCES ${DROGON_SOURCES}  
                orm_lib/src/mysql_impl/MysqlConnection.cc
                orm_lib/src/mysql_impl/MysqlResultImpl.cc)
        endif()
    endif()

    #Find sqlite3.
    find_package (SQLite3)
    if (SQLITE3_FOUND)
        message(STATUS "SQLite3 inc:" ${SQLITE3_INCLUDE_DIRS})
        include_directories(${SQLITE3_INCLUDE_DIRS})
        link_libraries(${SQLITE3_LIBRARIES})
        set(DROGON_SOURCES ${DROGON_SOURCES}  
            orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
            orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc)
    endif()
endif()

add_custom_target(makeVersion)
add_custom_command(TARGET makeVersion
        COMMAND ${PROJECT_SOURCE_DIR}/get_version.sh
        ARGS ${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h 
             ${DROGON_MAJOR_VERSION}
             ${DROGON_MINOR_VERSION}
             ${DROGON_PATCH_VERSION}
             ${PRE_RELEASE_STRING}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM )

if(NOT BUILD_EXAMPLES)
  set(BUILD_EXAMPLES TRUE CACHE BOOL INTERNAL)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(NOT BUILD_CTL)
  set(BUILD_CTL TRUE CACHE BOOL INTERNAL)
endif()

if(BUILD_CTL)
    add_subdirectory(drogon_ctl)
endif()

if(PostgreSQL_FOUND OR MYSQL_FOUND OR SQLITE3_FOUND)
    set(DROGON_SOURCES ${DROGON_SOURCES}  
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
else()
    set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/DbClientManagerSkipped.cc)
endif()

add_library(drogon STATIC ${DROGON_SOURCES})

set_property(TARGET drogon PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET drogon PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET drogon PROPERTY CXX_EXTENSIONS OFF)

add_dependencies(drogon makeVersion)

if(PostgreSQL_FOUND OR MYSQL_FOUND OR SQLITE3_FOUND)
    if(PostgreSQL_FOUND)
        OPTION(USE_POSTGRESQL "Enable PostgreSQL" ON)
    else()
        OPTION(USE_POSTGRESQL "Disable PostgreSQL" OFF)
    endif()

    if(MYSQL_FOUND)
        OPTION(USE_MYSQL "Enable Mysql" ON)
    else()
        OPTION(USE_MYSQL "DisableMysql" OFF)
    endif()

    if(SQLITE3_FOUND)
        OPTION(USE_SQLITE3 "Enable Sqlite3" ON)
    else()
        OPTION(USE_SQLITE3 "Disable Sqlite3" OFF)
    endif()
endif()

set(COMPILER_COMMAND ${CMAKE_CXX_COMPILER})
set(COMPILER_ID ${CMAKE_CXX_COMPILER_ID})


if(CMAKE_BUILD_TYPE)
    string(TOLOWER ${CMAKE_BUILD_TYPE} _type)
    if(_type STREQUAL release)
        set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -std=c++")
    ELSEif(_type STREQUAL debug)
        set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++")
    else()
        set(COMPILATION_FLAGS "-std=c++")
    endif()
else()
    set(COMPILATION_FLAGS "-std=c++")
endif()

get_target_property(INS drogon INCLUDE_DIRECTORIES)
list(APPEND INS "${INSTALL_INCLUDE_DIR}")
list(REMOVE_DUPLICATES INS)
set(INS_STRING "")
foreach(loop_var ${INS})
    set(INS_STRING "${INS_STRING} -I${loop_var}")
endforeach(loop_var)

set(INCLUDING_DIRS ${INS_STRING})

configure_file(${PROJECT_SOURCE_DIR}/cmake/templates/config.h.in 
               ${PROJECT_BINARY_DIR}/drogon/config.h @ONLY)

if (MAKETEST STREQUAL YES)
    add_subdirectory(lib/tests)
    if(PostgreSQL_FOUND)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl/test)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/tests)
    endif()
    if(MYSQL_FOUND)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/mysql_impl/test)
    endif()
    if(SQLITE3_FOUND)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/sqlite3_impl/test)
    endif()
endif ()

#Installation

install(TARGETS drogon EXPORT DrogonTargets
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)

set(DROGON_HEADERS 
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
    lib/inc/drogon/drogon_callbacks.h)
install(FILES ${DROGON_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon)

set(ORM_HEADERS 
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
install(FILES ${ORM_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/orm)

set(DROGON_UTIL_HEADERS 
    lib/inc/drogon/utils/ClassTraits.h
    lib/inc/drogon/utils/FunctionTraits.h
    lib/inc/drogon/utils/Utilities.h
    lib/inc/drogon/utils/any.h
    lib/inc/drogon/utils/string_view.h
    lib/inc/drogon/utils/HttpConstraint.h)
install(FILES ${DROGON_UTIL_HEADERS}
        DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils)

set(DROGON_PLUGIN_HEADERS lib/inc/drogon/plugins/Plugin.h)
install(FILES ${DROGON_PLUGIN_HEADERS}
        DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/plugins)

source_group( "Public API" FILES ${DROGON_HEADERS} 
                                 ${ORM_HEADERS}
                                 ${DROGON_UTIL_HEADERS}
                                 ${DROGON_PLUGIN_HEADERS})



# Add all targets to the build-tree export set
export(TARGETS drogon
  FILE "${PROJECT_BINARY_DIR}/DrogonTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global cmake-registry)
# export(PACKAGE Drogon)

include(CMakePackageConfigHelpers)
# ... for the build tree(locally included)
set(CONF_INCLUDE_DIRS   "${PROJECT_SOURCE_DIR}/lib/inc"
                        "${PROJECT_SOURCE_DIR}/orm_lib/inc"
                        "${PROJECT_SOURCE_DIR}/trantor"
                        "${JSONCPP_INCLUDE_DIRS}")
set(TRANTOR_CMAKE_DIR "${CMAKE_CURRENT_BINARY_DIR}/trantor")
configure_package_config_file(cmake/templates/DrogonConfig.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/DrogonConfig.cmake
                              INSTALL_DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
                              PATH_VARS CONF_INCLUDE_DIRS TRANTOR_CMAKE_DIR)
# ... for the install tree
if(JSONCPP_INCLUDE_DIRS STREQUAL INSTALL_INCLUDE_DIR)
    set(CONF_INCLUDE_DIRS   "${INSTALL_INCLUDE_DIR}")
else()
    set(CONF_INCLUDE_DIRS   "${JSONCPP_INCLUDE_DIRS}"
                            "${INSTALL_INCLUDE_DIR}")
endif()
set(TRANTOR_CMAKE_DIR "${INSTALL_LIB_DIR}/cmake/Trantor")
configure_package_config_file(cmake/templates/DrogonConfig.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake
                              INSTALL_DESTINATION ${INSTALL_DROGON_CMAKE_DIR}
                              PATH_VARS CONF_INCLUDE_DIRS TRANTOR_CMAKE_DIR)

# version
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/DrogonConfigVersion.cmake
                                 VERSION ${DROGON_VERSION}
                                 COMPATIBILITY SameMajorVersion)

# Install the DrogonConfig.cmake and DrogonConfigVersion.cmake
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/DrogonConfigVersion.cmake"
        DESTINATION "${INSTALL_DROGON_CMAKE_DIR}" 
        COMPONENT dev)

# Install the export set for use with the install-tree
install(EXPORT DrogonTargets 
        DESTINATION "${INSTALL_DROGON_CMAKE_DIR}" 
        COMPONENT dev)
