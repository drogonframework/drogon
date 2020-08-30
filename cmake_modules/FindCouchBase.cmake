# * Find the native libcouchbase headers and libraries.
#
# COUCHBASE_INCLUDE_DIRS    - where to find libcouchbase.h, etc.
# COUCHBASE_LIBRARIES - List of libraries when using sqlite. CouchBase_FOUND   -
# True if sqlite3 found. CouchBase_lib - The imported target library.

# Look for the header file.
find_path(COUCHBASE_INCLUDE_DIRS
          NAMES libcouchbase/couchbase.h
          DOC "libcouchbase include dir"
          PATH_SUFFIXES libcouchbase)

# Look for the library.
find_library(COUCHBASE_LIBRARIES NAMES couchbase)

# Handle the QUIETLY and REQUIRED arguments and set CouchBase_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CouchBase
                                  DEFAULT_MSG
                                  COUCHBASE_LIBRARIES
                                  COUCHBASE_INCLUDE_DIRS)

# Copy the results to the output variables.
if(CouchBase_FOUND)
  add_library(CouchBase_lib INTERFACE IMPORTED)
  set_target_properties(CouchBase_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${COUCHBASE_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES
                                   "${COUCHBASE_LIBRARIES}")
else(CouchBase_FOUND)
  set(COUCHBASE_LIBRARIES)
  set(COUCHBASE_INCLUDE_DIRS)
endif(CouchBase_FOUND)

mark_as_advanced(COUCHBASE_INCLUDE_DIRS COUCHBASE_LIBRARIES)
