link_libraries(${PROJECT_NAME})

add_executable(sqlite3_test1 test1.cc Groups.cc)

set_property(TARGET sqlite3_test1 PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET sqlite3_test1 PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET sqlite3_test1 PROPERTY CXX_EXTENSIONS OFF)
