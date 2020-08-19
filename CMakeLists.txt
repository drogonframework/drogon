cmake_minimum_required(VERSION 3.5)

project(drogon)
message(STATUS "compiler: " ${CMAKE_CXX_COMPILER_ID})
option(BUILD_CTL "Build drogon_ctl" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_ORM "Build orm" ON)
option(COZ_PROFILING "Use coz for profiling" OFF)
option(LIBPQ_BATCH_MODE "Use batch mode for libpq" ON)
option(BUILD_DROGON_SHARED "Build drogon as a shared lib" OFF)

set(DROGON_MAJOR_VERSION 1)
set(DROGON_MINOR_VERSION 0)
set(DROGON_PATCH_VERSION 0)
set(DROGON_VERSION
    ${DROGON_MAJOR_VERSION}.${DROGON_MINOR_VERSION}.${DROGON_PATCH_VERSION})
set(DROGON_VERSION_STRING "${DROGON_VERSION}.beta21")

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR
    include
    CACHE PATH "Installation directory for header files")
set(DEF_INSTALL_DROGON_CMAKE_DIR lib/cmake/Drogon)
set(INSTALL_DROGON_CMAKE_DIR
    ${DEF_INSTALL_DROGON_CMAKE_DIR}
    CACHE PATH "Installation directory for cmake files")

if(BUILD_DROGON_SHARED)
  set(BUILD_TRANTOR_SHARED TRUE)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
            "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")
  endif("${isSystemDir}" STREQUAL "-1")
  add_library(${PROJECT_NAME} SHARED)
else(BUILD_DROGON_SHARED)
  add_library(${PROJECT_NAME} STATIC)
endif(BUILD_DROGON_SHARED)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
if(HAS_ANY AND HAS_STRING_VIEW)
  set(DROGON_CXX_STANDARD 17)
else(HAS_ANY AND HAS_STRING_VIEW)
  set(DROGON_CXX_STANDARD 14)
endif(HAS_ANY AND HAS_STRING_VIEW)

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

add_subdirectory(trantor)

target_link_libraries(${PROJECT_NAME} PUBLIC trantor)

if(NOT WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
else(NOT WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)
endif(NOT WIN32)

if(DROGON_CXX_STANDARD LESS 17)
  # With C++14, use boost to support any and string_view
  message(STATUS "use c++14")
  find_package(Boost 1.61.0 REQUIRED)
  message(STATUS "boost include dir:" ${Boost_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PUBLIC Boost::boost)
  list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${Boost_INCLUDE_DIR})
else(DROGON_CXX_STANDARD LESS 17)
  message(STATUS "use c++17")
endif(DROGON_CXX_STANDARD LESS 17)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

# jsoncpp
find_package(Jsoncpp REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Jsoncpp_lib)
list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW ${JSONCPP_INCLUDE_DIRS})

if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" AND NOT WIN32)
  find_package(UUID REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE UUID_lib)

  try_compile(normal_uuid ${CMAKE_BINARY_DIR}/cmaketest
              ${PROJECT_SOURCE_DIR}/cmake/tests/normal_uuid_lib_test.cc
              LINK_LIBRARIES UUID_lib)
  try_compile(ossp_uuid ${CMAKE_BINARY_DIR}/cmaketest
              ${PROJECT_SOURCE_DIR}/cmake/tests/ossp_uuid_lib_test.cc
              LINK_LIBRARIES UUID_lib)
  if(normal_uuid)
    add_definitions(-DUSE_OSSP_UUID=0)
  elseif(ossp_uuid)
    add_definitions(-DUSE_OSSP_UUID=1)
  else()
    message(FATAL_ERROR "uuid lib error")
  endif()
endif(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" AND NOT WIN32)

find_package(Brotli)
if(Brotli_FOUND)
  message(STATUS "Brotli found")
  add_definitions(-DUSE_BROTLI)
  target_link_libraries(${PROJECT_NAME} PRIVATE Brotli_lib)
endif(Brotli_FOUND)

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
    lib/src/SecureSSLRedirector.cc
    lib/src/SessionManager.cc
    lib/src/StaticFileRouter.cc
    lib/src/Utilities.cc
    lib/src/WebSocketClientImpl.cc
    lib/src/WebSocketConnectionImpl.cc
    lib/src/WebsocketControllersRouter.cc)

if(NOT WIN32)
  set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/SharedLibManager.cc)
else(NOT WIN32)
  set(DROGON_SOURCES ${DROGON_SOURCES} third_party/mman-win32/mman.c)
endif(NOT WIN32)

if(BUILD_ORM)
  # find postgres
  find_package(pg)
  if(pg_FOUND)
    message(STATUS "libpq inc path:" ${PG_INCLUDE_DIRS})
    message(STATUS "libpq lib:" ${PG_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE pg_lib)
    set(DROGON_SOURCES ${DROGON_SOURCES}
                       orm_lib/src/postgresql_impl/PostgreSQLResultImpl.cc)
    if(LIBPQ_BATCH_MODE)
      try_compile(libpq_supports_batch ${CMAKE_BINARY_DIR}/cmaketest
                  ${PROJECT_SOURCE_DIR}/cmake/tests/test_libpq_batch_mode.cc
                  LINK_LIBRARIES ${PostgreSQL_LIBRARIES}
                  CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PostgreSQL_INCLUDE_DIR}")
    endif(LIBPQ_BATCH_MODE)
    if(libpq_supports_batch)
      message(STATUS "The libpq supports batch mode")
      option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" ON)
      set(DROGON_SOURCES ${DROGON_SOURCES}
                         orm_lib/src/postgresql_impl/PgBatchConnection.cc)
    else(libpq_supports_batch)
      option(LIBPQ_SUPPORTS_BATCH_MODE "libpq batch mode" OFF)
      set(DROGON_SOURCES ${DROGON_SOURCES}
                         orm_lib/src/postgresql_impl/PgConnection.cc)
    endif(libpq_supports_batch)
  endif(pg_FOUND)

  # Find mysql, only mariadb client liberary is supported
  find_package(MySQL)
  if(MySQL_FOUND)
    message(STATUS "Ok! We find the mariadb!")
    target_link_libraries(${PROJECT_NAME} PRIVATE MySQL_lib)
    set(DROGON_SOURCES ${DROGON_SOURCES}
                       orm_lib/src/mysql_impl/MysqlConnection.cc
                       orm_lib/src/mysql_impl/MysqlResultImpl.cc)
  endif(MySQL_FOUND)

  # Find sqlite3.
  find_package(SQLite3)
  if(SQLite3_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE SQLite3_lib)
    set(DROGON_SOURCES ${DROGON_SOURCES}
                       orm_lib/src/sqlite3_impl/Sqlite3Connection.cc
                       orm_lib/src/sqlite3_impl/Sqlite3ResultImpl.cc)
  endif(SQLite3_FOUND)
endif(BUILD_ORM)

find_package(ZLIB REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)

find_package(OpenSSL)
if(OpenSSL_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else(OpenSSL_FOUND)
  set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/ssl_funcs/Md5.cc
                     lib/src/ssl_funcs/Sha1.cc)
endif(OpenSSL_FOUND)

execute_process(COMMAND "git" rev-parse HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_SHA1
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/version.h.in"
               "${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h" @ONLY)

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif(BUILD_EXAMPLES)

if(BUILD_CTL)
  add_subdirectory(drogon_ctl)
endif(BUILD_CTL)

if(COZ_PROFILING)
  find_package(coz-profiler REQUIRED)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DCOZ_PROFILING=1)
  # If linked will not need to be ran with `coz run --- [executable]` to run the
  # profiler, but drogon_ctl currently won't build because it doesn't find debug
  # information while trying to generate it's own sources
  # target_link_libraries(${PROJECT_NAME} PUBLIC coz::coz)
  target_include_directories(${PROJECT_NAME} PUBLIC ${COZ_INCLUDE_DIRS})
endif(COZ_PROFILING)

if(pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
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
else(pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
  set(DROGON_SOURCES ${DROGON_SOURCES} lib/src/DbClientManagerSkipped.cc)
endif(pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)

target_sources(${PROJECT_NAME} PRIVATE ${DROGON_SOURCES})

set_target_properties(${PROJECT_NAME}
                      PROPERTIES CXX_STANDARD ${DROGON_CXX_STANDARD})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME Drogon)

if(pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
  if(pg_FOUND)
    option(USE_POSTGRESQL "Enable PostgreSQL" ON)
  else(pg_FOUND)
    option(USE_POSTGRESQL "Disable PostgreSQL" OFF)
  endif(pg_FOUND)

  if(MySQL_FOUND)
    option(USE_MYSQL "Enable Mysql" ON)
  else(MySQL_FOUND)
    option(USE_MYSQL "DisableMysql" OFF)
  endif(MySQL_FOUND)

  if(SQLite3_FOUND)
    option(USE_SQLITE3 "Enable Sqlite3" ON)
  else(SQLite3_FOUND)
    option(USE_SQLITE3 "Disable Sqlite3" OFF)
  endif(SQLite3_FOUND)
endif(pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)

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
else(CMAKE_BUILD_TYPE)
  set(COMPILATION_FLAGS "-std=c++")
endif(CMAKE_BUILD_TYPE)

list(APPEND INCLUDE_DIRS_FOR_DYNAMIC_VIEW
            "${CMAKE_INSTALL_PREFIX}/${INSTALL_INCLUDE_DIR}")
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
  if(pg_FOUND)
    add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl/test)
  endif(pg_FOUND)
  if(MySQL_FOUND)
    add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/mysql_impl/test)
  endif(MySQL_FOUND)
  if(SQLite3_FOUND)
    add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/sqlite3_impl/test)
  endif(SQLite3_FOUND)
  if(pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
    add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/tests)
  endif(pg_FOUND OR MySQL_FOUND OR SQLite3_FOUND)
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
    lib/inc/drogon/drogon_callbacks.h
    lib/inc/drogon/PubSubService.h)
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
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindUUID.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindJsoncpp.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindSQLite3.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindMySQL.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/Findpg.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindBrotli.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/Findcoz-profiler.cmake"
  DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
  COMPONENT dev)

# Install the export set for use with the install-tree
install(EXPORT DrogonTargets
        DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
        NAMESPACE Drogon::
        COMPONENT dev)
