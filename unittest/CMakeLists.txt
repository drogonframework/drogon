add_executable(msgbuffer_unittest MsgBufferUnittest.cpp)
add_executable(timer_unittest TimerUnittest.cpp)
add_executable(drobject_unittest DrObjectUnittest.cpp)
add_executable(gzip_unittest GzipUnittest.cpp)

set(UNITTEST_TARGETS
    msgbuffer_unittest
    timer_unittest
    drobject_unittest
    gzip_unittest)
set_property(TARGET ${UNITTEST_TARGETS}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${UNITTEST_TARGETS} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${UNITTEST_TARGETS} PROPERTY CXX_EXTENSIONS OFF)

include(GoogleTest)
foreach(T ${UNITTEST_TARGETS})
  target_link_libraries(${T} PRIVATE drogon gtest)
  gtest_discover_tests(${T})
endforeach()
