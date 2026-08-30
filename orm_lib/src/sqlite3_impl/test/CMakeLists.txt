link_libraries(${PROJECT_NAME})

add_executable(sqlite3_test1 test1.cc Groups.cc)

set_property(TARGET sqlite3_test1 PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET sqlite3_test1 PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET sqlite3_test1 PROPERTY CXX_EXTENSIONS OFF)

add_executable(sqlite3_disconnect_test Sqlite3DisconnectTest.cc)
set_property(TARGET sqlite3_disconnect_test
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET sqlite3_disconnect_test
             PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET sqlite3_disconnect_test PROPERTY CXX_EXTENSIONS OFF)
if (TARGET SQLite3_lib)
    target_link_libraries(sqlite3_disconnect_test PRIVATE SQLite3_lib)
else ()
    target_link_libraries(sqlite3_disconnect_test
                          PRIVATE unofficial::sqlite3::sqlite3)
endif ()
add_test(NAME sqlite3_disconnect COMMAND sqlite3_disconnect_test)
