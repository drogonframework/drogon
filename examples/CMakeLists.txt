link_libraries(${PROJECT_NAME})

set(benchmark_sources benchmark/BenchmarkCtrl.cc benchmark/JsonCtrl.cc
                      benchmark/main.cc)

add_executable(client client_example/main.cc)
add_executable(websocket_client websocket_client/WebSocketClient.cc)
add_executable(benchmark ${benchmark_sources})

set(example_targets
    benchmark
    client
    websocket_client)

set_property(TARGET ${example_targets}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${example_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${example_targets} PROPERTY CXX_EXTENSIONS OFF)
