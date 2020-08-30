link_libraries(${PROJECT_NAME})

add_executable(couchbase_test1 test1.cc)

set_property(TARGET couchbase_test1
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET couchbase_test1 PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET couchbase_test1 PROPERTY CXX_EXTENSIONS OFF)
