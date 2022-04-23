# * Try to find UUID Once done this will define
#
# UUID_FOUND - system has UUID 
# UUID_INCLUDE_DIRS - the UUID include directory
# UUID_LIBRARIES - Link these to use UUID UUID_DEFINITIONS - Compiler switches
# required for using UUID
#
# Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#
# Redistribution and use is allowed according to the terms of the New BSD
# license. For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if(UUID_LIBRARIES AND UUID_INCLUDE_DIRS)
  # in cache already
  set(UUID_FOUND TRUE)
else()
  find_path(
    UUID_INCLUDE_DIR
    NAMES uuid.h
    PATH_SUFFIXES uuid
    HINTS ${UUID_DIR}/include
          $ENV{UUID_DIR}/include
          $ENV{UUID_DIR}
          ${DELTA3D_EXT_DIR}/inc
          $ENV{DELTA_ROOT}/ext/inc
          $ENV{DELTA_ROOT}
    PATHS
      ~/Library/Frameworks
      /Library/Frameworks
      /usr/local/include
      /usr/include
      /usr/include/gdal
      /sw/include # Fink
      /opt/local/include # DarwinPorts
      /opt/csw/include # Blastwave
      /opt/include
      [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;OSG_ROOT]/include
      /usr/freeware/include)

  find_library(UUID_LIBRARY
               NAMES uuid ossp-uuid
               HINTS ${UUID_DIR}/lib
                     $ENV{UUID_DIR}/lib
                     $ENV{UUID_DIR}
                     ${DELTA3D_EXT_DIR}/lib
                     $ENV{DELTA_ROOT}/ext/lib
                     $ENV{DELTA_ROOT}
                     $ENV{OSG_ROOT}/lib
               PATHS ~/Library/Frameworks
                     /Library/Frameworks
                     /usr/local/lib
                     /usr/lib
                     /sw/lib
                     /opt/local/lib
                     /opt/csw/lib
                     /opt/lib
                     /usr/freeware/lib64)

  find_library(UUID_LIBRARY_DEBUG
               NAMES uuidd
               HINTS ${UUID_DIR}/lib
                     $ENV{UUID_DIR}/lib
                     $ENV{UUID_DIR}
                     ${DELTA3D_EXT_DIR}/lib
                     $ENV{DELTA_ROOT}/ext/lib
                     $ENV{DELTA_ROOT}
                     $ENV{OSG_ROOT}/lib
               PATHS ~/Library/Frameworks
                     /Library/Frameworks
                     /usr/local/lib
                     /usr/lib
                     /sw/lib
                     /opt/local/lib
                     /opt/csw/lib
                     /opt/lib
                     /usr/freeware/lib64)

  if(NOT UUID_LIBRARY AND (BSD OR APPLE))
    set(UUID_LIBRARY "")
  endif()

  set(UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIR})
  set(UUID_LIBRARIES ${UUID_LIBRARY})

  if(UUID_INCLUDE_DIRS)
    if((BSD OR APPLE) OR UUID_LIBRARIES)
      set(UUID_FOUND TRUE)
    endif()
  endif()

  if(UUID_FOUND)
    if(NOT UUID_FIND_QUIETLY)
      message(STATUS "Found UUID: ${UUID_LIBRARIES}")
    endif()
  else()
    if(UUID_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find UUID")
    endif()
  endif()

  # show the UUID_INCLUDE_DIRS and UUID_LIBRARIES variables only in the advanced
  # view
  mark_as_advanced(UUID_INCLUDE_DIRS UUID_LIBRARIES)

endif()

if(UUID_FOUND)
  add_library(UUID_lib INTERFACE IMPORTED)
  set_target_properties(UUID_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${UUID_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES
                                   "${UUID_LIBRARIES}")
else()
  set(UUID_LIBRARIES)
  set(UUID_INCLUDE_DIRS)
endif()
