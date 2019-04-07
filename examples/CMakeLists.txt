link_libraries(drogon trantor pthread dl)

FILE(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/*.csp)
foreach(cspFile ${SCP_LIST})
    message(STATUS "cspFile:" ${cspFile})
    EXEC_PROGRAM(basename ARGS "${cspFile} .csp" OUTPUT_VARIABLE classname)
    message(STATUS "view classname:" ${classname})
    add_custom_command(OUTPUT ${classname}.h ${classname}.cc
        COMMAND drogon_ctl
        ARGS create view ${cspFile}
        DEPENDS ${cspFile}
        VERBATIM )
   set(VIEWSRC ${VIEWSRC} ${classname}.cc)
endforeach()


AUX_SOURCE_DIRECTORY(simple_example DIR_STATIC)

add_executable(webapp ${DIR_STATIC} ${VIEWSRC})
add_dependencies(webapp drogon_ctl)

AUX_SOURCE_DIRECTORY(client_example DIR_CLIENT)
AUX_SOURCE_DIRECTORY(benchmark DIR_BENCHMARK)
#AUX_SOURCE_DIRECTORY(simple_example_test DIR_TEST)

add_executable(client ${DIR_CLIENT})
add_executable(benchmark ${DIR_BENCHMARK})
add_executable(webapp_test simple_example_test/main.cc)
add_executable(pipelining_test simple_example_test/HttpPipeliningTest.cc)
add_executable(websocket_test simple_example_test/WebSocketTest.cc)

add_custom_command(TARGET webapp POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                   ${PROJECT_SOURCE_DIR}/config.example.json ${PROJECT_SOURCE_DIR}/drogon.jpg 
                   ${PROJECT_SOURCE_DIR}/trantor/trantor/tests/server.pem $<TARGET_FILE_DIR:webapp>)