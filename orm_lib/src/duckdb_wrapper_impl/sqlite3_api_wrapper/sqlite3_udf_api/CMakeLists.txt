# set(UDF_WRAPPER_FILES cast_sqlite.cpp)
include_directories(include)

add_library(sqlite3_udf_api OBJECT sqlite3_udf_wrapper.cpp cast_sqlite.cpp)
set_target_properties(sqlite3_udf_api PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:sqlite3_udf_api>
    PARENT_SCOPE)
