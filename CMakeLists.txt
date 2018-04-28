cmake_minimum_required (VERSION 2.6)

project (DROGON C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fpermissive -g -ggdb")

#EXEC_PROGRAM (gcc ARGS "--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g'" OUTPUT_VARIABLE version)
#MESSAGE(STATUS "This is gcc version:: " ${version})

#if (version LESS 6.0.0)
#    MESSAGE(STATUS "old gcc")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fpermissive -g -ggdb")
#else()
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fpermissive -g -ggdb")
#    MESSAGE(STATUS "new gcc")
#endif()
add_subdirectory(trantor)

SET(CMAKE_INSTALL_PREFIX /usr/local/drogon)
#INSTALL(FILES trantor.cfg DESTINATION conf)

include_directories(${PROJECT_SOURCE_DIR}/trantor ${PROJECT_SOURCE_DIR}/drogon/inc)

aux_source_directory(${PROJECT_SOURCE_DIR}/drogon/src DIR_SRCS)

#add_library(db ${DIR_DB_SRCS})
add_executable(drogon ${DIR_SRCS})
add_dependencies(drogon trantor)

#if (version LESS 6.0.0)
#    MESSAGE(STATUS "old gcc...")
#    set (LIBCONFIG config++)
#else()
#    MESSAGE(STATUS "new gcc...")
#    set (LIBCONFIG libconfig++)
#endif()

target_link_libraries(drogon trantor pthread)
#crypt dl m pqxx pq hiredis mongocxx bsoncxx jsoncpp ${LIBCONFIG})
INSTALL(TARGETS drogon DESTINATION bin)
