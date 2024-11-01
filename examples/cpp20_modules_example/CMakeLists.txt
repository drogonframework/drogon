cmake_minimum_required(VERSION 3.28)

project(modules_example)

set(CMAKE_CXX_STANDARD 20)

# Default to C++ extensions being off. Clang's modules support have trouble
# with extensions right now.
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(jsoncpp REQUIRED)
find_package(OpenSSL REQUIRED)
# set(CMAKE_PREFIX_PATH /root/github/drogon/build/nqf/lib/cmake/Drogon;/root/github/drogon/build/nqf/lib/cmake/Trantor)
# find_package(Drogon REQUIRED)

add_executable(module_client module_client.cpp)
target_link_libraries(module_client drogon OpenSSL::SSL OpenSSL::Crypto uuid pthread dl)

add_executable(module_std module_std.cpp)
target_link_libraries(module_std PUBLIC drogon OpenSSL::SSL OpenSSL::Crypto uuid pthread dl)

add_executable(module_server module_server.cpp)
target_link_libraries(module_server PUBLIC drogon OpenSSL::SSL OpenSSL::Crypto uuid pthread dl)
