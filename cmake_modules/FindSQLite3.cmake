# Copyright (C) 2007-2009 LuaDist. Created by Peter Kapec <kapecp@gmail.com>
# Redistribution and use of this file is allowed according to the terms of the
# MIT license. For details see the COPYRIGHT file distributed with LuaDist.
# Note: Searching headers and libraries is very simple and is NOT as powerful as
# scripts distributed with CMake, because LuaDist defines directories to search
# for. Everyone is encouraged to contact the author with improvements. Maybe
# this file becomes part of CMake distribution sometimes.

# * Find sqlite3 Find the native SQLITE3 headers and libraries.
#
# SQLITE3_INCLUDE_DIRS    - where to find sqlite3.h, etc. 
# SQLITE3_LIBRARIES - List of libraries when using sqlite. 
# SQLite3_FOUND   - True if sqlite3 found.
# SQLite3_lib - The imported target library.

# Look for the header file.
find_path(SQLITE3_INCLUDE_DIRS NAMES sqlite3.h)

# Look for the library.
find_library(SQLITE3_LIBRARIES NAMES sqlite3)

# Handle the QUIETLY and REQUIRED arguments and set SQLite3_FOUND to TRUE if all
# listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3
                                  DEFAULT_MSG
                                  SQLITE3_LIBRARIES
                                  SQLITE3_INCLUDE_DIRS)

# Copy the results to the output variables.
if(SQLite3_FOUND)
  add_library(SQLite3_lib INTERFACE IMPORTED)
  set_target_properties(SQLite3_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${SQLITE3_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES
                                   "${SQLITE3_LIBRARIES}")
else(SQLite3_FOUND)
  set(SQLITE3_LIBRARIES)
  set(SQLITE3_INCLUDE_DIRS)
endif(SQLite3_FOUND)

mark_as_advanced(SQLITE3_INCLUDE_DIRS SQLITE3_LIBRARIES)
