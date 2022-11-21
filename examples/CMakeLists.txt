link_libraries(${PROJECT_NAME})

set(benchmark_sources benchmark/BenchmarkCtrl.cc benchmark/JsonCtrl.cc
                      benchmark/main.cc)

add_executable(client client_example/main.cc)
add_executable(websocket_client websocket_client/WebSocketClient.cc)
add_executable(websocket_server websocket_server/WebSocketServer.cc)
add_executable(benchmark ${benchmark_sources})
add_executable(helloworld helloworld/main.cc
                          helloworld/HelloController.cc
                          helloworld/HelloViewController.cc)
drogon_create_views(helloworld
                    ${CMAKE_CURRENT_SOURCE_DIR}/helloworld
                    ${CMAKE_CURRENT_BINARY_DIR})
add_executable(file_upload file_upload/file_upload.cc)
drogon_create_views(file_upload
                    ${CMAKE_CURRENT_SOURCE_DIR}/file_upload
                    ${CMAKE_CURRENT_BINARY_DIR})
add_executable(login_session login_session/main.cc)
drogon_create_views(login_session
                    ${CMAKE_CURRENT_SOURCE_DIR}/login_session
                    ${CMAKE_CURRENT_BINARY_DIR})

add_executable(jsonstore jsonstore/main.cc)

add_executable(redis_simple redis/main.cc
                            redis/controllers/Client.cc
                            redis/controllers/WsClient.cc)

add_executable(redis_chat redis_chat/main.cc
                          redis_chat/controllers/Chat.cc)

set(example_targets
    benchmark
    client
    websocket_client
    websocket_server
    helloworld
    file_upload
    login_session
    jsonstore
    redis_simple
    redis_chat)

# Add warnings for our example targets--some warnings (such as -Wunused-parameter) only appear
# when the templated functions are instantiated at their point of use.
if (CMAKE_CXX_COMPILER_ID MATCHES GNU)
    foreach(target ${example_targets})
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endforeach()
endif()

set_property(TARGET ${example_targets}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${example_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${example_targets} PROPERTY CXX_EXTENSIONS OFF)
