cmake_minimum_required (VERSION 3.2)

project (DROGON CXX)
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    EXEC_PROGRAM (gcc ARGS "--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g' | sed s'/-.*//g'" OUTPUT_VARIABLE version)
    MESSAGE(STATUS "This is gcc version:: " ${version})
    if(version LESS 4.9.0)
        MESSAGE(STATUS "gcc is too old")
        stop()
    elseif (version LESS 6.1.0)
        MESSAGE(STATUS "c++11")
        set(CMAKE_CXX_STD_FLAGS c++11)
    elseif(version LESS 7.1.0)
        set(CMAKE_CXX_STD_FLAGS c++14)
        MESSAGE(STATUS "c++14")
    else()
        set(CMAKE_CXX_STD_FLAGS c++17)
        set(DR_DEFS "USE_STD_ANY")
        MESSAGE(STATUS "c++17")
    endif()
else()
    set(CMAKE_CXX_STD_FLAGS c++11)
endif()



set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules/)
#jsoncpp
find_package (Jsoncpp REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIRS})
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

find_package (OpenSSL)
if(OpenSSL_FOUND)
    #add_definitions(-DUSE_OPENSSL)
    set(DR_DEFS "USE_OPENSSL;${DR_DEFS}")
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()
message(STATUS "openssl inc path:" ${OPENSSL_INCLUDE_DIR})
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
find_package(Boost)
if(Boost_FOUND)
    # add_definitions(-DUSE_BOOST)
    set(DR_DEFS "USE_BOOST;${DR_DEFS}")
    include_directories(${Boost_INCLUDE_DIRS})
endif()

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -std=${CMAKE_CXX_STD_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -std=${CMAKE_CXX_STD_FLAGS}")

add_subdirectory(trantor)
include_directories(${PROJECT_SOURCE_DIR}/trantor ${PROJECT_SOURCE_DIR}/lib/inc)

add_custom_target(makeVersion)
add_custom_command(TARGET makeVersion
        COMMAND ${PROJECT_SOURCE_DIR}/get_version.sh
        ARGS ${PROJECT_SOURCE_DIR}/lib/inc/drogon/version.h
        VERBATIM )

add_subdirectory(examples)
add_subdirectory(drogon_ctl)

aux_source_directory(${PROJECT_SOURCE_DIR}/lib/src DIR_SRCS)

ADD_LIBRARY(drogon ${DIR_SRCS})

add_dependencies(drogon trantor makeVersion)


SET(CONFIG_HEADER "${PROJECT_SOURCE_DIR}/config.h")
file(WRITE "${CONFIG_HEADER}" "#pragma once\n")

#get_directory_property(DR_DEFS COMPILE_DEFINITIONS)
foreach(loop_var ${DR_DEFS})
    file(APPEND "${CONFIG_HEADER}" "#define ${loop_var}\n")
endforeach(loop_var)
file(APPEND "${CONFIG_HEADER}" "#ifdef USE_STD_ANY\n")

file(APPEND "${CONFIG_HEADER}" "#include <any>\n")
file(APPEND "${CONFIG_HEADER}" "using std::any;\n")
file(APPEND "${CONFIG_HEADER}" "using std::any_cast;\n")

file(APPEND "${CONFIG_HEADER}" "#else \n")
file(APPEND "${CONFIG_HEADER}" "#ifdef USE_BOOST\n")

file(APPEND "${CONFIG_HEADER}" "#include <boost/any.hpp>\n")
file(APPEND "${CONFIG_HEADER}" "using boost::any;\n")
file(APPEND "${CONFIG_HEADER}" "using boost::any_cast;\n")

file(APPEND "${CONFIG_HEADER}" "#else\n")
file(APPEND "${CONFIG_HEADER}" "#error,must use c++17 or boost to support any\n")
file(APPEND "${CONFIG_HEADER}" "#endif\n")
file(APPEND "${CONFIG_HEADER}" "#endif\n")
string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LOWER)
if(CMAKE_BUILD_TYPE_LOWER STREQUAL release)
    file(APPEND "${CONFIG_HEADER}" "\n" "const char compileFlags[]=\"" ${CMAKE_CXX_FLAGS_RELEASE} "\";")
else()
    file(APPEND "${CONFIG_HEADER}" "\n" "const char compileFlags[]=\"" ${CMAKE_CXX_FLAGS_DEBUG} "\";")
endif()
get_target_property(INS drogon INCLUDE_DIRECTORIES)
file(APPEND "${CONFIG_HEADER}" "\nconst char includeDirs[]=\"")
foreach(loop_var ${INS})
    file(APPEND "${CONFIG_HEADER}" "-I" ${loop_var} " ")
endforeach(loop_var)
file(APPEND "${CONFIG_HEADER}" "\";\n")
EXEC_PROGRAM(${PROJECT_SOURCE_DIR}/update_config.sh ARGS "${CONFIG_HEADER} ${PROJECT_SOURCE_DIR}/lib/inc/drogon/config.h")

if (MAKETEST STREQUAL YES)
    ADD_SUBDIRECTORY(tests)
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

#target_link_libraries(drogon trantor pthread)
