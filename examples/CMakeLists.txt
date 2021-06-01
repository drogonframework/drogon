link_libraries(${PROJECT_NAME})

set(benchmark_sources benchmark/BenchmarkCtrl.cc benchmark/JsonCtrl.cc
                      benchmark/main.cc)

add_executable(client client_example/main.cc)
add_executable(websocket_client websocket_client/WebSocketClient.cc)
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

set(example_targets
    benchmark
    client
    websocket_client
    helloworld
    file_upload
    login_session)

set_property(TARGET ${example_targets}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${example_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${example_targets} PROPERTY CXX_EXTENSIONS OFF)
