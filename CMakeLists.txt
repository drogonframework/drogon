cmake_minimum_required (VERSION 2.6)

project (DROGON CXX)

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
EXEC_PROGRAM (gcc ARGS "--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g' | sed s'/-.*//g'" OUTPUT_VARIABLE version)
MESSAGE(STATUS "This is gcc version:: " ${version})
if(version LESS 4.8.5)
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
    #add_definitions(-DUSE_STD_ANY)
    set(DEFS "USE_STD_ANY")
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

find_package (OpenSSL)
if(OpenSSL_FOUND)
#add_definitions(-DUSE_OPENSSL)
set(DEFS "USE_OPENSSL;${DEFS}")
include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(${OPENSSL_SSL_LIBRARY})
link_directories(${OPENSSL_CRYPTO_LIBRARY})
endif()
message(STATUS "openssl inc path:" ${OPENSSL_INCLUDE_DIR})

find_package(Boost)
if(Boost_FOUND)
   # add_definitions(-DUSE_BOOST)
    set(DEFS "USE_BOOST;${DEFS}")
    include_directories(${Boost_INCLUDE_DIRS})
endif()


set(CMAKE_CXX_FLAGS "-std=${CMAKE_CXX_STD_FLAGS} -g -Wall -fpermissive ${CMAKE_CXX_FLAGS}")

#string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

MESSAGE(STATUS ${CMAKE_CXX_FLAGS})



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

SET(CMAKE_INSTALL_PREFIX /usr/local)

install(TARGETS drogon DESTINATION lib)

file(GLOB trantor_net_headers "${CMAKE_CURRENT_SOURCE_DIR}/lib/inc/drogon/*.h")
install(FILES ${trantor_net_headers} DESTINATION include/drogon)
add_dependencies(drogon trantor makeVersion)


SET(CONFIG_HEADER "${PROJECT_SOURCE_DIR}/config.h")
file(WRITE "${CONFIG_HEADER}" "#pragma once\n")

#get_directory_property(DEFS COMPILE_DEFINITIONS)
foreach(loop_var ${DEFS})
    file(APPEND "${CONFIG_HEADER}" "#define ${loop_var}\n")
endforeach(loop_var)
#file(APPEND "${CONFIG_HEADER}" "const char definitions[]=\"")
#
#
#foreach(loop_var ${DEFS})
##message(STATUS "definitions " ${loop_var})
#file(APPEND "${CONFIG_HEADER}" "-D" ${loop_var} " ")
#endforeach(loop_var)
#file(APPEND "${CONFIG_HEADER}" "\";")
file(APPEND "${CONFIG_HEADER}" "\n" "const char compileFlags[]=\"" ${CMAKE_CXX_FLAGS} "\";")

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

#target_link_libraries(drogon trantor pthread)
