cmake_minimum_required(VERSION 3.5)
project(trantor)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules/)

set(TRANTOR_MAJOR_VERSION 1)
set(TRANTOR_MINOR_VERSION 1)
set(TRANTOR_PATCH_VERSION 1)
set(TRANTOR_VERSION
    ${TRANTOR_MAJOR_VERSION}.${TRANTOR_MINOR_VERSION}.${TRANTOR_PATCH_VERSION})

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_INCLUDE_DIR
    include
    CACHE PATH "Installation directory for header files")
set(DEF_INSTALL_TRANTOR_CMAKE_DIR lib/cmake/Trantor)
set(INSTALL_TRANTOR_CMAKE_DIR
    ${DEF_INSTALL_TRANTOR_CMAKE_DIR}
    CACHE PATH "Installation directory for cmake files")

if(BUILD_TRANTOR_SHARED)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
            "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")
  endif("${isSystemDir}" STREQUAL "-1")
  add_library(${PROJECT_NAME} SHARED)
  if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    # Ignore MSVC C4251 and C4275 warning of exporting std objects with no dll export
    # We export class to facilitate maintenance, thus if you compile
    # drogon on windows as a shared library, you will need to use
    # exact same compiler for drogon and your app.
    target_compile_options(${PROJECT_NAME} PUBLIC /wd4251 /wd4275)
  endif()
else(BUILD_TRANTOR_SHARED)
  add_library(${PROJECT_NAME} STATIC)
endif(BUILD_TRANTOR_SHARED)

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/exports/trantor/exports.h)

# include directories
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
         $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/exports>
  PRIVATE ${PROJECT_SOURCE_DIR}
          ${PROJECT_SOURCE_DIR}/trantor/utils
          ${PROJECT_SOURCE_DIR}/trantor/net
          ${PROJECT_SOURCE_DIR}/trantor/net/inner
          $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/wepoll>)

if(MINGW)
  target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC -D_WIN32_WINNT=0x0601)
endif(MINGW)

set(TRANTOR_SOURCES
    trantor/utils/AsyncFileLogger.cc
    trantor/utils/ConcurrentTaskQueue.cc
    trantor/utils/Date.cc
    trantor/utils/LogStream.cc
    trantor/utils/Logger.cc
    trantor/utils/MsgBuffer.cc
    trantor/utils/SerialTaskQueue.cc
    trantor/utils/TimingWheel.cc
    trantor/net/EventLoop.cc
    trantor/net/EventLoopThread.cc
    trantor/net/EventLoopThreadPool.cc
    trantor/net/InetAddress.cc
    trantor/net/TcpClient.cc
    trantor/net/TcpServer.cc
    trantor/net/Channel.cc
    trantor/net/inner/Acceptor.cc
    trantor/net/inner/Connector.cc
    trantor/net/inner/Poller.cc
    trantor/net/inner/Socket.cc
    trantor/net/inner/TcpConnectionImpl.cc
    trantor/net/inner/Timer.cc
    trantor/net/inner/TimerQueue.cc
    trantor/net/inner/poller/EpollPoller.cc
    trantor/net/inner/poller/KQueue.cc)

if(WIN32)
  set(TRANTOR_SOURCES ${TRANTOR_SOURCES} trantor/utils/WindowsSupport.cc)
  set(TRANTOR_SOURCES ${TRANTOR_SOURCES} third_party/wepoll/Wepoll.c)
endif(WIN32)

find_package(OpenSSL)
if(OpenSSL_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_OPENSSL)
endif()

find_package(c-ares)
if(c-ares_FOUND)
  message(STATUS "c-ares found!")
  target_link_libraries(${PROJECT_NAME} PRIVATE c-ares_lib)
  set(TRANTOR_SOURCES ${TRANTOR_SOURCES} trantor/net/inner/AresResolver.cc)
else()
  set(TRANTOR_SOURCES ${TRANTOR_SOURCES} trantor/net/inner/NormalResolver.cc)
endif()
target_sources(${PROJECT_NAME} PRIVATE ${TRANTOR_SOURCES})

find_package(Threads)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 Rpcrt4)
  if(OpenSSL_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Crypt32 Secur32)
  endif(OpenSSL_FOUND)
endif(WIN32)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 14)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME Trantor)

if(BUILD_TESTING)
  add_subdirectory(trantor/tests)
  find_package(GTest)
  if(GTest_FOUND)
    enable_testing()
    add_subdirectory(trantor/unittests)
  endif()
endif()

set(public_net_headers
    trantor/net/EventLoop.h
    trantor/net/EventLoopThread.h
    trantor/net/EventLoopThreadPool.h
    trantor/net/InetAddress.h
    trantor/net/TcpClient.h
    trantor/net/TcpConnection.h
    trantor/net/TcpServer.h
    trantor/net/callbacks.h
    trantor/net/Resolver.h
    trantor/net/Channel.h)

set(public_utils_headers
    trantor/utils/AsyncFileLogger.h
    trantor/utils/ConcurrentTaskQueue.h
    trantor/utils/Date.h
    trantor/utils/Funcs.h
    trantor/utils/LockFreeQueue.h
    trantor/utils/LogStream.h
    trantor/utils/Logger.h
    trantor/utils/MsgBuffer.h
    trantor/utils/NonCopyable.h
    trantor/utils/ObjectPool.h
    trantor/utils/SerialTaskQueue.h
    trantor/utils/TaskQueue.h
    trantor/utils/TimingWheel.h)

source_group("Public API"
             FILES
             ${public_net_headers}
             ${public_utils_headers})

install(TARGETS trantor
                # IMPORTANT: Add the trantor library to the "export-set"
        EXPORT TrantorTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/exports/trantor/exports.h
        DESTINATION ${INSTALL_INCLUDE_DIR}/trantor)
install(FILES ${public_net_headers}
        DESTINATION ${INSTALL_INCLUDE_DIR}/trantor/net)
install(FILES ${public_utils_headers}
        DESTINATION ${INSTALL_INCLUDE_DIR}/trantor/utils)

include(CMakePackageConfigHelpers)
# ... for the install tree
configure_package_config_file(
  cmake/templates/TrantorConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TrantorConfig.cmake
  INSTALL_DESTINATION
  ${INSTALL_TRANTOR_CMAKE_DIR})

# version
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/TrantorConfigVersion.cmake
  VERSION ${TRANTOR_VERSION}
  COMPATIBILITY SameMajorVersion)

# Install the TrantorConfig.cmake and TrantorConfigVersion.cmake
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TrantorConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TrantorConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/Findc-ares.cmake"
  DESTINATION "${INSTALL_TRANTOR_CMAKE_DIR}"
  COMPONENT dev)

# Install the export set for use with the install-tree
install(EXPORT TrantorTargets
        DESTINATION "${INSTALL_TRANTOR_CMAKE_DIR}"
        NAMESPACE Trantor::
        COMPONENT dev)
