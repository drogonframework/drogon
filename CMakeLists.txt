cmake_minimum_required (VERSION 2.6)

project (DROGON CXX)



EXEC_PROGRAM (gcc ARGS "--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g'" OUTPUT_VARIABLE version)
MESSAGE(STATUS "This is gcc version:: " ${version})

if (version LESS 6.0.0)
    MESSAGE(STATUS "c++11")
    set(CMAKE_CXX_STD_FLAGS c++11)
elseif(version LESS 7.1.0)
    set(CMAKE_CXX_STD_FLAGS c++14)
    MESSAGE(STATUS "c++14")
else()
    set(CMAKE_CXX_STD_FLAGS c++17)
    add_definitions(-DUSE_C++17)
    MESSAGE(STATUS "c++17")
endif()

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=${CMAKE_CXX_STD_FLAGS} -fpermissive -g -ggdb)

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")


set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules/)

find_package (jsoncpp REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIRS})
link_directories(${JSONCPP_LIBRARY_DIRS})
find_package (UUID)

if(UUID_FOUND)
    add_definitions(-DUSE_UUID)
endif()

find_package(Boost)
if(BOOST_FOUND)
    add_definitions(-DUSE_BOOST)
    include_directories(${Boost_INCLUDE_DIRS})
endif()
add_subdirectory(trantor)

include_directories(${PROJECT_SOURCE_DIR}/trantor ${PROJECT_SOURCE_DIR}/lib/inc)

add_subdirectory(examples)

aux_source_directory(${PROJECT_SOURCE_DIR}/lib/src DIR_SRCS)

ADD_LIBRARY(drogon ${DIR_SRCS})

SET(CMAKE_INSTALL_PREFIX /usr/local)

install(TARGETS drogon DESTINATION lib)

file(GLOB trantor_net_headers "${CMAKE_CURRENT_SOURCE_DIR}/lib/inc/drogon/*.h")
install(FILES ${trantor_net_headers} DESTINATION include/drogon)

add_dependencies(drogon trantor)

#target_link_libraries(drogon trantor pthread)
