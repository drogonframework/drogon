find_path(COZ_INCLUDE_DIRS NAMES coz.h)

find_library(COZ_LIBRARIES NAMES coz PATH_SUFFIXES coz-profiler)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(coz-profiler
                                  DEFAULT_MSG
                                  COZ_LIBRARIES
                                  COZ_INCLUDE_DIRS)

if(COZ-PROFILER_FOUND)
  add_library(coz::coz INTERFACE IMPORTED)
  set_target_properties(coz::coz
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   ${COZ_INCLUDE_DIRS}
                                   INTERFACE_LINK_LIBRARIES
                                   ${COZ_LIBRARIES})
else(COZ-PROFILER_FOUND)
  set(COZ_LIBRARIES)
  set(COZ_INCLUDE_DIRS)
endif(COZ-PROFILER_FOUND)

mark_as_advanced(COZ_INCLUDE_DIRS COZ_LIBRARIES)
