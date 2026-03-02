link_libraries(${PROJECT_NAME})

add_executable(duckdb_test duck_test.cc)

set_property(TARGET duckdb_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET duckdb_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET duckdb_test PROPERTY CXX_EXTENSIONS OFF)
