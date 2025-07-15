cmake_minimum_required(VERSION 3.15)
project(backend_www)

set(BACKEND_WWW_ROOT_HEADERS
)
set(BACKEND_WWW_ROOT_FILES
    main.cc
)

add_executable(${PROJECT_NAME}
    ${BACKEND_WWW_ROOT_HEADERS} ${BACKEND_WWW_ROOT_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)
