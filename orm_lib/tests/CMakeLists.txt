link_libraries(${PROJECT_NAME})
if (WIN32)
    link_libraries(iphlpapi)
endif (WIN32)

add_executable(db_test
        db_test.cc
        postgresql/Users.cc
		postgresql/Wallets.cc
        postgresql/Blog.cc
		postgresql/Category.cc
        postgresql/BlogTag.cc
        postgresql/Tag.cc
        mysql/Users.cc
		mysql/Wallets.cc
        mysql/Blog.cc
		mysql/Category.cc
        mysql/BlogTag.cc
        mysql/Tag.cc
        sqlite3/Users.cc
		sqlite3/Wallets.cc
        sqlite3/Blog.cc
		sqlite3/Category.cc
        sqlite3/BlogTag.cc
        sqlite3/Tag.cc
        )

if (WIN32)
    target_compile_options(db_test PRIVATE /bigobj)
endif (WIN32)

add_executable(pipeline_test
        pipeline_test.cpp
        )

add_executable(db_listener_test
        db_listener_test.cc
        )

add_executable(conn_options_test
		conn_options_test.cc
        )

add_executable(db_api_test
		db_api_test.cc
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

set_property(TARGET conn_options_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET conn_options_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET conn_options_test PROPERTY CXX_EXTENSIONS OFF)

set_property(TARGET db_api_test PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET db_api_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET db_api_test PROPERTY CXX_EXTENSIONS OFF)
