add_executable(msgbuffer_unittest MsgBufferUnittest.cpp)
add_executable(drobject_unittest DrObjectUnittest.cpp)
add_executable(gzip_unittest GzipUnittest.cpp)
add_executable(md5_unittest MD5Unittest.cpp ../lib/src/ssl_funcs/Md5.cc)
add_executable(sha1_unittest SHA1Unittest.cpp ../lib/src/ssl_funcs/Sha1.cc)
add_executable(ostringstream_unittest OStringStreamUnitttest.cpp)

set(UNITTEST_TARGETS
    msgbuffer_unittest
    drobject_unittest
    gzip_unittest
    md5_unittest
    sha1_unittest
    ostringstream_unittest)

set_property(TARGET ${UNITTEST_TARGETS}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${UNITTEST_TARGETS} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${UNITTEST_TARGETS} PROPERTY CXX_EXTENSIONS OFF)

include(GoogleTest)
foreach(T ${UNITTEST_TARGETS})
  target_link_libraries(${T} PRIVATE drogon gtest)
  gtest_discover_tests(${T})
endforeach()
