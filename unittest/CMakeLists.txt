add_executable(drogon_msgbuffer_unittest MsgBufferUnittest.cpp)
add_executable(drobject_unittest DrObjectUnittest.cpp)
add_executable(gzip_unittest GzipUnittest.cpp)
add_executable(md5_unittest MD5Unittest.cpp ../lib/src/ssl_funcs/Md5.cc)
add_executable(sha1_unittest SHA1Unittest.cpp ../lib/src/ssl_funcs/Sha1.cc)
add_executable(ostringstream_unittest OStringStreamUnitttest.cpp)
add_executable(base64_unittest Base64Unittest.cpp)
if(Brotli_FOUND)
  add_executable(brotli_unittest BrotliUnittest.cpp)
endif()

set(UNITTEST_TARGETS
    drogon_msgbuffer_unittest
    drobject_unittest
    gzip_unittest
    md5_unittest
    sha1_unittest
    ostringstream_unittest
    base64_unittest)
if(Brotli_FOUND)
  set(UNITTEST_TARGETS ${UNITTEST_TARGETS} brotli_unittest)
endif()

set_property(TARGET ${UNITTEST_TARGETS}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${UNITTEST_TARGETS} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${UNITTEST_TARGETS} PROPERTY CXX_EXTENSIONS OFF)

include(GoogleTest)
foreach(T ${UNITTEST_TARGETS})
  target_link_libraries(${T} PRIVATE drogon GTest::GTest)
  gtest_discover_tests(${T})
endforeach()
