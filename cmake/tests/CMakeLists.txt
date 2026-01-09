cmake_minimum_required(VERSION 3.5)
project(UUIDTests CXX)

# This CMakeLists.txt is used by try_compile to test UUID library variants
add_executable(normal_uuid_lib_test normal_uuid_lib_test.cc)
add_executable(ossp_uuid_lib_test ossp_uuid_lib_test.cc)
