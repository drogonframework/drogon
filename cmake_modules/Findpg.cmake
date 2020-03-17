# Find PostgreSQL
#
# Find the PostgreSQL includes and library
#
# This module defines PG_INCLUDE_DIRS, where to find header, etc. PG_LIBRARIES,
# the libraries needed to use PostgreSQL. pg_FOUND, If false, do not try to use
# PostgreSQL. 

find_package(PostgreSQL)
if(PostgreSQL_FOUND)
  set(PG_LIBRARIES ${PostgreSQL_LIBRARIES})
  set(PG_INCLUDE_DIRS ${PostgreSQL_INCLUDE_DIR})
  add_library(pg_lib INTERFACE IMPORTED)
  set_target_properties(pg_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   ${PostgreSQL_INCLUDE_DIR}
                                   INTERFACE_LINK_LIBRARIES
                                   ${PostgreSQL_LIBRARIES})
  mark_as_advanced(PG_INCLUDE_DIRS PG_LIBRARIES)
  set(pg_FOUND TRUE)
endif()
