link_libraries(${PROJECT_NAME})
if(WIN32)
  link_libraries(iphlpapi)
endif(WIN32)

add_executable(db_test
    db_test.cc
    postgresql/Users.cc
    mysql/Users.cc
    sqlite3/Users.cc
)

add_executable(pipeline_test
        pipeline_test.cpp
)

add_executable(db_listener_test
        db_listener_test.cc
)

set_property(TARGET db_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET db_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET db_test PROPERTY CXX_EXTENSIONS OFF)

set_property(TARGET pipeline_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET pipeline_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET pipeline_test PROPERTY CXX_EXTENSIONS OFF)

set_property(TARGET db_listener_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET db_listener_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET db_listener_test PROPERTY CXX_EXTENSIONS OFF)
