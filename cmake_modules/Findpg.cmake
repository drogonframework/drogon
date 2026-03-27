# Find PostgreSQL
#
# Find the PostgreSQL includes and library
#
# This module defines PG_INCLUDE_DIRS, where to find header, etc. PG_LIBRARIES,
# the libraries needed to use PostgreSQL. pg_FOUND, If false, do not try to use
# PostgreSQL.
# pg_lib - The imported target library.

# On Apple Silicon Macs, Homebrew installs to /opt/homebrew instead of
# /usr/local (Intel Macs). Detect the prefix dynamically so cmake finds
# dependencies regardless of Mac architecture.
if(APPLE)
    execute_process(
        COMMAND brew --prefix libpq
        OUTPUT_VARIABLE HOMEBREW_PG_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(HOMEBREW_PG_PREFIX)
        list(APPEND CMAKE_PREFIX_PATH ${HOMEBREW_PG_PREFIX})
    endif()
endif()

find_package(PostgreSQL)
if(PostgreSQL_FOUND)
  set(PG_LIBRARIES ${PostgreSQL_LIBRARIES})
  set(PG_INCLUDE_DIRS ${PostgreSQL_INCLUDE_DIRS})
  message(STATUS "pg inc: " ${PostgreSQL_INCLUDE_DIRS})
  add_library(pg_lib INTERFACE IMPORTED)
  set_target_properties(pg_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${PostgreSQL_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES
                                   "${PostgreSQL_LIBRARIES}")
  mark_as_advanced(PG_INCLUDE_DIRS PG_LIBRARIES)
endif(PostgreSQL_FOUND)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(pg
                                  DEFAULT_MSG
                                  PG_LIBRARIES
                                  PG_INCLUDE_DIRS)
