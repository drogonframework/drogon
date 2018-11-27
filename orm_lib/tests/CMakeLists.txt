link_libraries(drogon trantor pthread dl)

add_executable(db_test db_test.cc users.cc)