link_libraries(${PROJECT_NAME})

file(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/*.csp)
foreach(cspFile ${SCP_LIST})
  message(STATUS "cspFile:" ${cspFile})
  exec_program(basename
               ARGS
               "${cspFile} .csp"
               OUTPUT_VARIABLE
               classname)
  message(STATUS "view classname:" ${classname})
  add_custom_command(OUTPUT ${classname}.h ${classname}.cc
                     COMMAND drogon_ctl
                             ARGS
                             create
                             view
                             ${cspFile}
                     DEPENDS ${cspFile}
                     VERBATIM)
  set(VIEWSRC ${VIEWSRC} ${classname}.cc)
endforeach()

set(simple_example_sources
    simple_example/CustomCtrl.cc
    simple_example/CustomHeaderFilter.cc
    simple_example/DoNothingPlugin.cc
    simple_example/ForwardCtrl.cc
    simple_example/JsonTestController.cc
    simple_example/ListParaCtl.cc
    simple_example/PipeliningTest.cc
    simple_example/TestController.cc
    simple_example/TestPlugin.cc
    simple_example/TestViewCtl.cc
    simple_example/WebSocketTest.cc
    simple_example/api_Attachment.cc
    simple_example/api_v1_ApiTest.cc
    simple_example/TimeFilter.cc
    simple_example/main.cc)

add_executable(webapp ${simple_example_sources} ${VIEWSRC})
add_dependencies(webapp drogon_ctl)

set(client_example_sources client_example/main.cc)
set(benchmark_sources benchmark/BenchmarkCtrl.cc benchmark/JsonCtrl.cc
                      benchmark/main.cc)
# AUX_SOURCE_DIRECTORY(simple_example_test DIR_TEST)

add_executable(client ${client_example_sources})
add_executable(benchmark ${benchmark_sources})
add_executable(webapp_test simple_example_test/main.cc)
add_executable(pipelining_test simple_example_test/HttpPipeliningTest.cc)
add_executable(websocket_test simple_example_test/WebSocketTest.cc)
add_executable(multiple_ws_test simple_example_test/MultipleWsTest.cc)

add_custom_command(TARGET webapp POST_BUILD
                   COMMAND gzip
                           ARGS
                           -c
                           ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/index.html
                           >
                           ${CMAKE_CURRENT_BINARY_DIR}/index.html.gz
                   VERBATIM)
add_custom_command(
  TARGET webapp POST_BUILD
  COMMAND ${CMAKE_COMMAND}
          -E
          copy_if_different
          ${PROJECT_SOURCE_DIR}/config.example.json
          ${PROJECT_SOURCE_DIR}/drogon.jpg
          ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/index.html
          ${PROJECT_SOURCE_DIR}/trantor/trantor/tests/server.pem
          $<TARGET_FILE_DIR:webapp>)

set(example_targets
    webapp
    webapp_test
    client
    benchmark
    pipelining_test
    websocket_test
    multiple_ws_test)

set_property(TARGET ${example_targets}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${example_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${example_targets} PROPERTY CXX_EXTENSIONS OFF)
