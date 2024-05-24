cmake_minimum_required(VERSION 3.15)
project(drogon_server)

set(DROGON_SERVER_ROOT_HEADERS
)
set(DROGON_SERVER_ROOT_FILES
    main.cc
)

set(DROGON_SERVER_PLUGINS_HEADERS
    plugins/ReverseProxy.h
)
set(DROGON_SERVER_PLUGINS_FILES
    plugins/ReverseProxy.cc
)

add_executable(${PROJECT_NAME}
    ${DROGON_SERVER_ROOT_HEADERS} ${DROGON_SERVER_ROOT_FILES}
    ${DROGON_SERVER_PLUGINS_HEADERS} ${DROGON_SERVER_PLUGINS_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)
