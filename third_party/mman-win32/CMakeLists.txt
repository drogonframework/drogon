project (mman-win32 C)

cmake_minimum_required (VERSION 3.5)

option (BUILD_TESTS "tests?" OFF)

set (headers mman.h)
set (sources mman.c)

add_library (mman STATIC ${sources})

set_target_properties(mman PROPERTIES EXPORT_NAME MMAN)

install (TARGETS mman 
					  EXPORT MMANTargets
					  RUNTIME DESTINATION bin
                      LIBRARY DESTINATION lib${LIB_SUFFIX}
                      ARCHIVE DESTINATION lib${LIB_SUFFIX})

install(EXPORT MMANTargets
        DESTINATION "${INSTALL_DROGON_CMAKE_DIR}"
        COMPONENT dev)

install (FILES ${headers} DESTINATION include/sys)

if (BUILD_TESTS)
  enable_testing ()
  add_executable (t_mman test.c)
  target_link_libraries (t_mman mman)
  add_test (NAME t_mman COMMAND t_mman${CMAKE_EXECUTABLE_SUFFIX})
endif ()


