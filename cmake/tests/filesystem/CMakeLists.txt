cmake_minimum_required(VERSION 3.10)
project(FilesystemStandardTest LANGUAGES CXX)

# Exercise the same find module shipped with Drogon's installed package.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../cmake_modules")
if(TEST_STANDARD STREQUAL "unset")
    unset(CMAKE_CXX_STANDARD)
    unset(CMAKE_CXX_STANDARD CACHE)
elseif(TEST_STANDARD STREQUAL "cache17")
    unset(CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "Consumer standard" FORCE)
    set(TEST_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD "${TEST_STANDARD}")
endif()

find_package(Filesystem COMPONENTS Final REQUIRED)

# A newly created target inherits CMAKE_CXX_STANDARD if it is defined.
# An empty value makes target_compile_features fail for downstream consumers.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cc" "int main() { return 0; }\n")
add_executable(consumer "${CMAKE_CURRENT_BINARY_DIR}/main.cc")
target_compile_features(consumer PRIVATE cxx_std_17)
target_link_libraries(consumer PRIVATE std::filesystem)

if(TEST_STANDARD STREQUAL "unset")
    if(DEFINED CMAKE_CXX_STANDARD)
        message(FATAL_ERROR "FindFilesystem defined an unset CMAKE_CXX_STANDARD")
    endif()
elseif(NOT CMAKE_CXX_STANDARD STREQUAL TEST_STANDARD)
    message(FATAL_ERROR "FindFilesystem changed CMAKE_CXX_STANDARD")
endif()
