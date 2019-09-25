link_libraries(${PROJECT_NAME})

add_executable(db_test db_test.cc Users.cc)

set_property(TARGET db_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET db_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET db_test PROPERTY CXX_EXTENSIONS OFF)
