cmake_minimum_required(VERSION 3.5)

project(drogon CXX)
message(STATUS "compiler: " ${CMAKE_CXX_COMPILER_ID})
option(BUILD_CTL "Build drogon_ctl" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_ORM "Build orm" ON)
option(LIBPQ_BATCH_MODE "Use batch mode for libpq" ON)
option(BUILD_DROGON_SHARED "Build drogon as a shared lib" OFF)

set(DROGON_MAJOR_VERSION 1)
set(DROGON_MINOR_VERSION 0)
set(DROGON_PATCH_VERSION 0)
set(DROGON_VERSION
    ${DROGON_MAJOR_VERSION}.${DROGON_MINOR_VERSION}.${DROGON_PATCH_VERSION})
set(DROGON_VERSION_STRING "${DROGON_VERSION}.beta14")

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR
    include
    CACHE PATH "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_DROGON_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_DROGON_CMAKE_DIR lib/cmake/Drogon)
endif()
set(INSTALL_DROGON_CMAKE_DIR
    ${DEF_INSTALL_DROGON_CMAKE_DIR}
    CACHE PATH "Installation directory for cmake files")
# Make relative paths absolute (needed later on)
foreach(p
        LIB
        BIN
        INCLUDE
        DROGON_CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

if(BUILD_DROGON_SHARED)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${INSTALL_LIB_DIR}"
            isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "${INSTALL_LIB_DIR}")
  endif("${isSystemDir}" STREQUAL "-1")
  add_library(${PROJECT_NAME} SHARED)
else()
  add_library(${PROJECT_NAME} STATIC)
endif()

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
if(HAS_ANY AND HAS_STRING_VIEW)
  set(DROGON_CXX_STANDARD 17)
else()
  set(DROGON_CXX_STANDARD 14)
endif()

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/inc>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/orm_lib/inc>
         $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/trantor>
         $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>)

if(WIN32)
  target_include_directories(
    ${PROJECT_NAME}
    PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/mman-win32>)
endif(WIN32)

# set the include list for dynamic view compiling
list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW
            ${PROJECT_SOURCE_DIR}/lib/inc
            ${PROJECT_SOURCE_DIR}/orm_lib/inc
            ${PROJECT_SOURCE_DIR}/trantor)

if(WIN32)
  add_subdirectory(third_party/mman-win32)
endif(WIN32)
add_subdirectory(trantor)

target_link_libraries(${PROJECT_NAME} PUBLIC trantor)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE mman)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif(WIN32)

if(DROGON_CXX_STANDARD LESS 17)
  # With C++14, use boost to support any and string_view
  message(STATUS "use c++14")
  find_package(Boost 1.61.0 REQUIRED)
  target_include_directories(${PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIRS})
  list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${Boost_INCLUDE_DIRS})
else()
  message(STATUS "use c++17")
endif()

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

# jsoncpp
find_package(Jsoncpp REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${JSONCPP_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${JSONCPP_LIBRARIES})
list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${JSONCPP_INCLUDE_DIRS})
message(STATUS "jsoncpp inc path:" ${JSONCPP_INCLUDE_DIRS})

# message(STATUS ${JSONCPP_LIBRARIES})
if(NOT EXISTS ${JSONCPP_INCLUDE_DIRS}/json/version.h)
  message(FATAL_ERROR "Error: jsoncpp lib is too old.....stop")
endif()

if(NOT WIN32)
  exec_program(
    cat
    ARGS
    "${JSONCPP_INCLUDE_DIRS}/json/version.h |grep JSONCPP_VERSION_STRING|sed s'/.*define/define/'|awk '{printf $3}'|sed s'/\"//g'"
    OUTPUT_VARIABLE
    jsoncpp_ver)
  message(STATUS "jsoncpp verson:" ${jsoncpp_ver})
  if(jsoncpp_ver LESS 1.7)
    message(
      FATAL_ERROR
        "jsoncpp lib is too old,please get new version from https://github.com/open-source-parsers/jsoncpp"
      )
  endif()

  find_package(UUID REQUIRED)
  target_include_directories(${PROJECT_NAME} PRIVATE ${UUID_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${UUID_LIBRARIES})

  try_compile(normal_uuid ${CMAKE_BINARY_DIR}/cmaketest
              ${PROJECT_SOURCE_DIR}/cmake/tests/normal_uuid_lib_test.cc
              LINK_LIBRARIES ${UUID_LIBRARIES}
              CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${UUID_INCLUDE_DIR}")
  try_compile(ossp_uuid ${CMAKE_BINARY_DIR}/cmaketest
              ${PROJECT_SOURCE_DIR}/cmake/tests/ossp_uuid_lib_test.cc
              LINK_LIBRARIES ${UUID_LIBRARIES}
              CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${UUID_INCLUDE_DIR}")
  if(normal_uuid)
    add_definitions(-DUSE_OSSP_UUID=0)
  elseif(ossp_uuid)
    add_definitions(-DUSE_OSSP_UUID=1)
  else()
    message(FATAL_ERROR "uuid lib error")
  endif()
endif(NOT WIN32)
if(NOT MSVC OR NOT BUILD_ORM)
  find_package(ZLIB REQUIRED)
  target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_LIBRARIES})
endif(NOT MSVC OR NOT BUILD_ORM)

set(DROGON_SOURCES
    lib/src/AOPAdvice.cc
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
    lib/src/SecureSSLRedirector.cc
    lib/src/SessionManager.cc
    lib/src/StaticFileRouter.cc
    lib/src/Utilities.cc
    lib/src/WebSocketClientImpl.cc
    lib/src/WebSocketConnectionImpl.cc
    lib/src/WebsocketControllersRouter.cc)

if(NOT WIN32)
  set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/SharedLibManager.cc)
endif(NOT WIN32)

find_package(OpenSSL)
if(OpenSSL_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSSL_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSSL_LIBRARIES})
else()
  set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/ssl_funcs/Md5.cc
                     lib/src/ssl_funcs/Sha1.cc)
endif()

if(BUILD_ORM)
  # find postgres
  find_package(PostgreSQL)
  if(PostgreSQL_FOUND)
    message(STATUS "libpq inc path:" ${PostgreSQL_INCLUDE_DIR})
    message(STATUS "libpq lib:" ${PostgreSQL_LIBRARIES})
    target_include_directories(${PROJECT_NAME}
                               PRIVATE ${PostgreSQL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PostgreSQL_LIBRARIES})
    set(DROGON_SOURCES ${DROGON_SOURCES}
                       orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc)
    if(LIBPQ_BATCH_MODE)
      try_compile(libpq_supports_batch ${CMAKE_BINARY_DIR}/cmaketest
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

  # Find mysql, only mariadb client liberary is supported
  find_package(MySQL)
  if(MYSQL_FOUND)
    message(STATUS "inc:" ${MYSQL_INCLUDE_DIR})
    message(STATUS "libs:" ${MYSQL_CLIENT_LIBS})
    message(STATUS "version:" ${MYSQL_VERSION_STRING})
    message(STATUS "Ok! We find the mariadb!")
    target_include_directories(${PROJECT_NAME} PRIVATE ${MYSQL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_CLIENT_LIBS})
    set(DROGON_SOURCES ${DROGON_SOURCES}
                       orm_lib/src/mysql_impl/MysqlConnection.cc
                       orm_lib/src/mysql_impl/MysqlResultImpl.cc)
  else(MYSQL_FOUND)
    if(MSVC)
      find_package(ZLIB REQUIRED)
      target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIR})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_LIBRARIES})
    endif(MSVC)
  endif(MYSQL_FOUND)

  # Find sqlite3.
  find_package(SQLite3)
  if(SQLITE3_FOUND)
    message(STATUS "SQLite3 inc:" ${SQLITE3_INCLUDE_DIRS})
    target_include_directories(${PROJECT_NAME} PRIVATE ${SQLITE3_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SQLITE3_LIBRARIES})
    set(DROGON_SOURCES ${DROGON_SOURCES}
                       orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
                       orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc)
  endif()
endif()

execute_process(COMMAND "git" rev-parse HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_SHA1
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/version.h.in"
               "${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h" @ONLY)

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_CTL)
  add_subdirectory(drogon_ctl)
endif()

if(PostgreSQL_FOUND OR MYSQL_FOUND OR SQLITE3_FOUND)
  set(DROGON_SOURCES
      ${DROGON_SOURCES}
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
      orm_lib/src/TransactionImpl.cc
      orm_lib/src/RestfulController.cc)
else()
  set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/DbClientManagerSkipped.cc)
endif()

target_sources(${PROJECT_NAME} PRIVATE ${DROGON_SOURCES})

set_target_properties(${PROJECT_NAME}
                      PROPERTIES CXX_STANDARD ${DROGON_CXX_STANDARD})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME Drogon)

if(PostgreSQL_FOUND OR MYSQL_FOUND OR SQLITE3_FOUND)
  if(PostgreSQL_FOUND)
    option(USE_POSTGRESQL "Enable PostgreSQL" ON)
  else()
    option(USE_POSTGRESQL "Disable PostgreSQL" OFF)
  endif()

  if(MYSQL_FOUND)
    option(USE_MYSQL "Enable Mysql" ON)
  else()
    option(USE_MYSQL "DisableMysql" OFF)
  endif()

  if(SQLITE3_FOUND)
    option(USE_SQLITE3 "Enable Sqlite3" ON)
  else()
    option(USE_SQLITE3 "Disable Sqlite3" OFF)
  endif()
endif()

set(COMPILER_COMMAND ${CMAKE_CXX_COMPILER})
set(COMPILER_ID ${CMAKE_CXX_COMPILER_ID})

if(CMAKE_BUILD_TYPE)
  string(TOLOWER ${CMAKE_BUILD_TYPE} _type)
  if(_type STREQUAL release)
    set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -std=c++")
  elseif(_type STREQUAL debug)
    set(COMPILATION_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -std=c++")
  else()
    set(COMPILATION_FLAGS "-std=c++")
  endif()
else()
  set(COMPILATION_FLAGS "-std=c++")
endif()

list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW "${INSTALL_INCLUDE_DIR}")
list(REMOVE_DUPLICATES INCLUDE_DIRS_FOR_DYNAMIC_VIEW)
set(INS_STRING "")
foreach(loop_var ${INCLUDE_DIRS_FOR_DYNAMIC_VIEW})
  set(INS_STRING "${INS_STRING} -I${loop_var}")
endforeach(loop_var)

set(INCLUDING_DIRS ${INS_STRING})

configure_file(${PROJECT_SOURCE_DIR}/cmake/templates/config.h.in
               ${PROJECT_BINARY_DIR}/drogon/config.h @ONLY)

if(BUILD_TESTING)
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
  find_package(GTest)
  if(GTest_FOUND)
    message(STATUS "gtest found")
    enable_testing()
    add_subdirectory(unittest)
  endif(GTest_FOUND)
endif(BUILD_TESTING)

# Installation

install(TARGETS ${PROJECT_NAME}
        EXPORT DrogonTargets
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)

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
    orm_lib/inc/drogon/orm/SqlBinder.h
    orm_lib/inc/drogon/orm/RestfulController.h)
install(FILES ${ORM_HEADERS} DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/orm)

set(DROGON_UTIL_HEADERS
    lib/inc/drogon/utils/FunctionTraits.h
    lib/inc/drogon/utils/Utilities.h
    lib/inc/drogon/utils/any.h
    lib/inc/drogon/utils/string_view.h
    lib/inc/drogon/utils/HttpConstraint.h
    lib/inc/drogon/utils/OStringStream.h)
install(FILES ${DROGON_UTIL_HEADERS}
        DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/utils)

set(DROGON_PLUGIN_HEADERS lib/inc/drogon/plugins/Plugin.h
                          lib/inc/drogon/plugins/SecureSSLRedirector.h)
install(FILES ${DROGON_PLUGIN_HEADERS}
        DESTINATION ${INSTALL_INCLUDE_DIR}/drogon/plugins)

source_group("Public API"
             FILES
             ${DROGON_HEADERS}
             ${ORM_HEADERS}
             ${DROGON_UTIL_HEADERS}
             ${DROGON_PLUGIN_HEADERS})

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
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/DrogonConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/DrogonConfigVersion.cmake"
  DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
  COMPONENT dev)

# Install the export set for use with the install-tree
install(EXPORT DrogonTargets
        DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
        NAMESPACE Drogon::
        COMPONENT dev)
