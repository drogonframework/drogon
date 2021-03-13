link_libraries(${PROJECT_NAME})

add_executable(redis_test
        redis_test.cc
        )

set_property(TARGET redis_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET redis_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET redis_test PROPERTY CXX_EXTENSIONS OFF)