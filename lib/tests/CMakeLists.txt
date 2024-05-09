link_libraries(${PROJECT_NAME})
if(WIN32)
  link_libraries(iphlpapi)
endif(WIN32)
set(UNITTEST_SOURCES
    unittests/main.cc
    unittests/Base64Test.cc
    unittests/UrlCodecTest.cc
    unittests/GzipTest.cc
    unittests/HttpViewDataTest.cc
    unittests/CookieTest.cc
    unittests/ClassNameTest.cc
    unittests/HttpDateTest.cc
    unittests/HttpHeaderTest.cc
    unittests/MD5Test.cc
    unittests/MsgBufferTest.cc
    unittests/OStringStreamTest.cc
    unittests/PubSubServiceUnittest.cc
    unittests/Sha1Test.cc
    unittests/FileTypeTest.cc
    unittests/DrObjectTest.cc
    unittests/HttpFullDateTest.cc
    unittests/MainLoopTest.cc
    unittests/CacheMapTest.cc
    unittests/StringOpsTest.cc
    unittests/ControllerCreationTest.cc
    unittests/MultiPartParserTest.cc
    unittests/SlashRemoverTest.cc
    unittests/UtilitiesTest.cc
    unittests/UuidUnittest.cc
)

if(DROGON_CXX_STANDARD GREATER_EQUAL 20 AND HAS_COROUTINE)
  set(UNITTEST_SOURCES ${UNITTEST_SOURCES} unittests/CoroutineTest.cc)
endif()

if(Brotli_FOUND)
  set(UNITTEST_SOURCES ${UNITTEST_SOURCES} unittests/BrotliTest.cc)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND BUILD_SHARED_LIBS)
  set(UNITTEST_SOURCES ${UNITTEST_SOURCES} ../src/HttpUtils.cc)
else()
  set(UNITTEST_SOURCES ${UNITTEST_SOURCES} ../src/HttpFileImpl.cc
                       unittests/HttpFileTest.cc)
endif()

add_executable(unittest ${UNITTEST_SOURCES})

if (BUILD_CTL)
  set(INTEGRATION_TEST_CLIENT_SOURCES
      integration_test/client/main.cc
      integration_test/client/WebSocketTest.cc
      integration_test/client/MultipleWsTest.cc
      integration_test/client/HttpPipeliningTest.cc)
  add_executable(integration_test_client ${INTEGRATION_TEST_CLIENT_SOURCES})

  set(INTEGRATION_TEST_SERVER_SOURCES
      integration_test/server/CustomCtrl.cc
      integration_test/server/CustomHeaderFilter.cc
      integration_test/server/DoNothingPlugin.cc
      integration_test/server/ForwardCtrl.cc
      integration_test/server/JsonTestController.cc
      integration_test/server/ListParaCtl.cc
      integration_test/server/PipeliningTest.cc
      integration_test/server/TestController.cc
      integration_test/server/TestPlugin.cc
      integration_test/server/TestViewCtl.cc
      integration_test/server/WebSocketTest.cc
      integration_test/server/api_Attachment.cc
      integration_test/server/api_v1_ApiTest.cc
      integration_test/server/TimeFilter.cc
      integration_test/server/DigestAuthFilter.cc
      integration_test/server/MethodTest.cc
      integration_test/server/RangeTestController.cc
      integration_test/server/BeginAdviceTest.cc
      integration_test/server/main.cc)

  if(DROGON_CXX_STANDARD GREATER_EQUAL 20 AND HAS_COROUTINE)
    set(INTEGRATION_TEST_SERVER_SOURCES
        ${INTEGRATION_TEST_SERVER_SOURCES}
        integration_test/server/CoroFilter.cpp
        integration_test/server/api_v1_CoroTest.cc)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
  endif(DROGON_CXX_STANDARD GREATER_EQUAL 20 AND HAS_COROUTINE)

  add_executable(integration_test_server ${INTEGRATION_TEST_SERVER_SOURCES})
  drogon_create_views(integration_test_server
                      ${CMAKE_CURRENT_SOURCE_DIR}/integration_test/server
                      ${CMAKE_CURRENT_BINARY_DIR})
  add_dependencies(integration_test_server drogon_ctl)
  add_custom_command(
    TARGET integration_test_server POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -E
            copy_if_different
            ${PROJECT_SOURCE_DIR}/config.example.json
            ${PROJECT_SOURCE_DIR}/drogon.jpg
            ${CMAKE_CURRENT_SOURCE_DIR}/integration_test/server/index.html
            ${CMAKE_CURRENT_SOURCE_DIR}/integration_test/server/main.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/integration_test/server/test.md
            ${CMAKE_CURRENT_SOURCE_DIR}/integration_test/server/index.html.gz
            ${CMAKE_CURRENT_SOURCE_DIR}/integration_test/server/中文.txt
            ${PROJECT_SOURCE_DIR}/trantor/trantor/tests/server.crt
            ${PROJECT_SOURCE_DIR}/trantor/trantor/tests/server.key
            $<TARGET_FILE_DIR:integration_test_server>)
  add_custom_command(
    TARGET integration_test_server POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -E
            copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/integration_test/server/a-directory
            $<TARGET_FILE_DIR:integration_test_server>/a-directory)
endif(BUILD_CTL)

set(COOKIE_SAME_SITE
  main_CookieSameSite.cc
  CookieSameSite.cc
)
add_executable(cookie_same_site ${COOKIE_SAME_SITE})

add_executable(real_ip_resolver RealIpResolverTest.cc)

set(tests unittest cookie_same_site real_ip_resolver)
if (BUILD_CTL)
  list(APPEND tests integration_test_server integration_test_client)
endif(BUILD_CTL)
set_property(TARGET ${tests} PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${tests} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${tests} PROPERTY CXX_EXTENSIONS OFF)

ParseAndAddDrogonTests(unittest)
ParseAndAddDrogonTests(cookie_same_site)
ParseAndAddDrogonTests(real_ip_resolver)
