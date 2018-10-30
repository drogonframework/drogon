cmake_minimum_required (VERSION 3.2)

project (DROGON CXX)
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    EXEC_PROGRAM (gcc ARGS "--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g' | sed s'/-.*//g'" OUTPUT_VARIABLE version)
    MESSAGE(STATUS "This is gcc version:: " ${version})
    if(version LESS 5.4.0)
        MESSAGE(STATUS "gcc is too old")
        stop()
    elseif(version LESS 7.1.0)
        set(CMAKE_CXX_STD_FLAGS c++14)
        MESSAGE(STATUS "c++14")
    else()
        set(CMAKE_CXX_STD_FLAGS c++17)
        MESSAGE(STATUS "c++17")
    endif()
else()
#MacOS use c++17
    set(CMAKE_CXX_STD_FLAGS c++17)
endif()

include_directories(${PROJECT_SOURCE_DIR}/trantor ${PROJECT_SOURCE_DIR}/lib/inc ${PROJECT_SOURCE_DIR}/orm_lib/inc)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules/)
#jsoncpp
find_package (Jsoncpp REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIRS})
link_libraries(${JSONCPP_LIBRARIES})

message(STATUS "jsoncpp inc path:" ${JSONCPP_INCLUDE_DIRS})

#message(STATUS ${JSONCPP_LIBRARIES})
if(NOT EXISTS ${JSONCPP_INCLUDE_DIRS}/json/version.h)
    message(STATUS "jsoncpp lib is too old.....stop")
    stop()
endif()
EXEC_PROGRAM(cat ARGS "${JSONCPP_INCLUDE_DIRS}/json/version.h |grep JSONCPP_VERSION_STRING|sed s'/.*define/define/'|awk '{printf $3}'|sed s'/\"//g'" OUTPUT_VARIABLE jsoncpp_ver)
message(STATUS "jsoncpp verson:" ${jsoncpp_ver})
if(jsoncpp_ver LESS 1.7)
    MESSAGE(STATUS "jsoncpp lib is too old,please get new version from https://github.com/open-source-parsers/jsoncpp")
    stop()
endif()

find_package (UUID REQUIRED)
include_directories(${UUID_INCLUDE_DIR})
link_libraries(${UUID_LIBRARIES})

find_package (OpenSSL)
if(OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    link_libraries(${OPENSSL_LIBRARIES})
    message(STATUS "openssl inc path:" ${OPENSSL_INCLUDE_DIR})
else()
    aux_source_directory(${PROJECT_SOURCE_DIR}/lib/src/ssl_funcs DIR_SRCS)
endif()


find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
link_libraries(${ZLIB_LIBRARIES})

message(STATUS "zlib inc path:" ${ZLIB_INCLUDE_DIR})

#find postgres
find_package(PostgreSQL)
if(PostgreSQL_FOUND)
include_directories(${PostgreSQL_INCLUDE_DIR})
message(STATUS "libpq inc path:" ${PostgreSQL_INCLUDE_DIR})
link_libraries(${PostgreSQL_LIBRARIES})
aux_source_directory(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl DIR_SRCS)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -std=${CMAKE_CXX_STD_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -std=${CMAKE_CXX_STD_FLAGS}")

add_subdirectory(trantor)

add_custom_target(makeVersion)
add_custom_command(TARGET makeVersion
        COMMAND ${PROJECT_SOURCE_DIR}/get_version.sh
        ARGS ${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h
        VERBATIM )

add_subdirectory(examples)
add_subdirectory(drogon_ctl)

aux_source_directory(${PROJECT_SOURCE_DIR}/lib/src DIR_SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/orm_lib/src DIR_SRCS)

ADD_LIBRARY(drogon ${DIR_SRCS})

add_dependencies(drogon trantor makeVersion)


SET(CONFIG_HEADER "${PROJECT_SOURCE_DIR}/config.h")
file(WRITE "${CONFIG_HEADER}" "#pragma once\n")

file(APPEND "${CONFIG_HEADER}" "#include <trantor/utils/config.h>\n")

if(PostgreSQL_FOUND)
file(APPEND "${CONFIG_HEADER}" "#define USE_POSTGRESQL 1")
else()
file(APPEND "${CONFIG_HEADER}" "#define USE_POSTGRESQL 0")
endif()

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LOWER)
if(CMAKE_BUILD_TYPE_LOWER STREQUAL release)
    file(APPEND "${CONFIG_HEADER}" "\n" "const char compileFlags[]=\"" ${CMAKE_CXX_FLAGS_RELEASE} "\";")
else()
    file(APPEND "${CONFIG_HEADER}" "\n" "const char compileFlags[]=\"" ${CMAKE_CXX_FLAGS_DEBUG} "\";")
endif()
get_target_property(INS drogon INCLUDE_DIRECTORIES)

file(APPEND "${CONFIG_HEADER}" "\nconst char includeDirs[]=\"")
set(TMP_INS,"")
foreach(loop_var ${INS})
    if(TMP_INS MATCHES ";${loop_var};")
    else()
        set(TMP_INS ";${loop_var};${TMP_INS}")
        file(APPEND "${CONFIG_HEADER}" "-I" ${loop_var} " ")
    endif()
endforeach(loop_var)

file(APPEND "${CONFIG_HEADER}" "\";\n")
EXEC_PROGRAM(${PROJECT_SOURCE_DIR}/update_config.sh ARGS "${CONFIG_HEADER} ${PROJECT_SOURCE_DIR}/lib/inc/drogon/config.h")

if (MAKETEST STREQUAL YES)
    ADD_SUBDIRECTORY(tests)
    if(PostgreSQL_FOUND)
        add_subdirectory(${PROJECT_SOURCE_DIR}/orm_lib/src/postgresql_impl/test)
    endif()
endif ()

SET(CMAKE_INSTALL_PREFIX /usr/local)

install(TARGETS drogon DESTINATION lib)

file(GLOB drogon_headers "${CMAKE_CURRENT_SOURCE_DIR}/lib/inc/drogon/*.h")
install(FILES ${drogon_headers} DESTINATION include/drogon)
file(GLOB drogon_util_headers "${CMAKE_CURRENT_SOURCE_DIR}/lib/inc/drogon/utils/*.h")
install(FILES ${drogon_util_headers}
        DESTINATION include/drogon/utils)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/lib/inc/drogon/version.h"
        DESTINATION include/drogon)
