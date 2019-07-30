LINK_LIBRARIES(drogon trantor pthread dl)

FILE(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/*.csp)
FOREACH(cspFile ${SCP_LIST})
    MESSAGE(STATUS "cspFile:" ${cspFile})
    EXEC_PROGRAM(basename ARGS "${cspFile} .csp" OUTPUT_VARIABLE classname)
    MESSAGE(STATUS "view classname:" ${classname})
    ADD_CUSTOM_COMMAND(OUTPUT ${classname}.h ${classname}.cc
        COMMAND drogon_ctl
        ARGS create view ${cspFile}
        DEPENDS ${cspFile}
        VERBATIM )
   SET(VIEWSRC ${VIEWSRC} ${classname}.cc)
ENDFOREACH()


SET(simple_example_sources  simple_example/CustomCtrl.cc
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

ADD_EXECUTABLE(webapp ${simple_example_sources} ${VIEWSRC})
ADD_DEPENDENCIES(webapp drogon_ctl)

SET(client_example_sources client_example/main.cc)
SET(benchmark_sources   benchmark/BenchmarkCtrl.cc
                        benchmark/JsonCtrl.cc
                        benchmark/main.cc)
#AUX_SOURCE_DIRECTORY(simple_example_test DIR_TEST)

ADD_EXECUTABLE(client ${client_example_sources})
ADD_EXECUTABLE(benchmark ${benchmark_sources})
ADD_EXECUTABLE(webapp_test simple_example_test/main.cc)
ADD_EXECUTABLE(pipelining_test simple_example_test/HttpPipeliningTest.cc)
ADD_EXECUTABLE(websocket_test simple_example_test/WebSocketTest.cc)

ADD_CUSTOM_COMMAND(TARGET webapp POST_BUILD
                   COMMAND gzip
                   ARGS -c ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/index.html > ${CMAKE_CURRENT_BINARY_DIR}/index.html.gz
                   VERBATIM)
ADD_CUSTOM_COMMAND(TARGET webapp POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                   ${PROJECT_SOURCE_DIR}/config.example.json ${PROJECT_SOURCE_DIR}/drogon.jpg 
                   ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/index.html
                   ${PROJECT_SOURCE_DIR}/trantor/trantor/tests/server.pem $<TARGET_FILE_DIR:webapp>)

SET(example_targets webapp webapp_test client benchmark pipelining_test websocket_test)

SET_PROPERTY(TARGET ${example_targets} PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
SET_PROPERTY(TARGET ${example_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
SET_PROPERTY(TARGET ${example_targets} PROPERTY CXX_EXTENSIONS OFF)
