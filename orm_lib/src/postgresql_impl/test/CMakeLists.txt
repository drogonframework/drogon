link_libraries(${PROJECT_NAME})

add_executable(pg_test1 test1.cc)
add_executable(pg_test2 test2.cc)

set_property(TARGET pg_test1 pg_test2
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET pg_test1 pg_test2 PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET pg_test1 pg_test2 PROPERTY CXX_EXTENSIONS OFF)
