# Find jsoncpp
#
# Find the jsoncpp includes and library
#
# if you nee to add a custom library search path, do it via via
# CMAKE_PREFIX_PATH
#
# This module defines JSONCPP_INCLUDE_DIRS, where to find header, etc.
# JSONCPP_LIBRARIES, the libraries needed to use jsoncpp. JSONCPP_FOUND, If
# false, do not try to use jsoncpp. 
# Jsoncpp_lib - The imported target library.

# only look in default directories
find_path(JSONCPP_INCLUDE_DIRS
          NAMES json/json.h
          DOC "jsoncpp include dir"
          PATH_SUFFIXES jsoncpp)

find_library(JSONCPP_LIBRARIES NAMES jsoncpp DOC "jsoncpp library")

# debug library on windows same naming convention as in qt (appending debug
# library with d) boost is using the same "hack" as us with "optimized" and
# "debug" if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
# find_library(JSONCPP_LIBRARIES_DEBUG NAMES jsoncppd DOC "jsoncpp debug
# library") if("${JSONCPP_LIBRARIES_DEBUG}" STREQUAL "JSONCPP_LIBRARIES_DEBUG-
# NOTFOUND") set(JSONCPP_LIBRARIES_DEBUG ${JSONCPP_LIBRARIES}) endif()

# set(JSONCPP_LIBRARIES optimized ${JSONCPP_LIBRARIES} debug
# ${JSONCPP_LIBRARIES_DEBUG})

# endif()

# handle the QUIETLY and REQUIRED arguments and set JSONCPP_FOUND to TRUE if all
# listed variables are TRUE, hide their existence from configuration view
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jsoncpp
                                  DEFAULT_MSG
                                  JSONCPP_INCLUDE_DIRS
                                  JSONCPP_LIBRARIES)
mark_as_advanced(JSONCPP_INCLUDE_DIRS JSONCPP_LIBRARIES)

if(Jsoncpp_FOUND)
  if(NOT EXISTS ${JSONCPP_INCLUDE_DIRS}/json/version.h)
    message(FATAL_ERROR "Error: jsoncpp lib is too old.....stop")
  endif()
  if(NOT WIN32)
    exec_program(
      cat
      ARGS
      "${JSONCPP_INCLUDE_DIRS}/json/version.h |grep JSONCPP_VERSION_STRING|sed s'/.*define/define/'|awk '{printf $3}'|sed s'/\"//g'"
      OUTPUT_VARIABLE
      jsoncpp_ver)
    message(STATUS "jsoncpp verson:" ${jsoncpp_ver})
    if(jsoncpp_ver LESS 1.7)
      message(
        FATAL_ERROR
          "jsoncpp lib is too old, please get new version from https://github.com/open-source-parsers/jsoncpp"
        )
    endif(jsoncpp_ver LESS 1.7)
  endif()
  add_library(Jsoncpp_lib INTERFACE IMPORTED)
  set_target_properties(Jsoncpp_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${JSONCPP_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES
                                   "${JSONCPP_LIBRARIES}")

endif(Jsoncpp_FOUND)
