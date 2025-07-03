include_directories(inc)
add_subdirectory(sqlite3)

add_definitions(-DSQLITE_SHELL_IS_UTF8)
add_definitions(-DUSE_DUCKDB_SHELL_WRAPPER)

include_directories(../../../../third_party/utf8proc/include)

# if(DUCKDB_EXTENSION_AUTOCOMPLETE_SHOULD_LINK)
#   include_directories(../../extension/autocomplete/include)
#   set(ALL_OBJECT_FILES ${ALL_OBJECT_FILES}
#                        ../../extension/autocomplete/autocomplete_extension.cpp)
#   add_definitions(-DSHELL_INLINE_AUTOCOMPLETE)
# endif()

set(SQLITE3_API_WRAPPER_SRCS
    shell_extension.cpp
    sqlite3_api_wrapper.cpp
)

add_library(sqlite3_api_wrapper_objs OBJECT ${SQLITE3_API_WRAPPER_SRCS})
set_target_properties(sqlite3_api_wrapper_objs PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:sqlite3_api_wrapper_objs>
    PARENT_SCOPE)

