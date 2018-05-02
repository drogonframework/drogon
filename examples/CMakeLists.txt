link_libraries(drogon trantor uuid pthread)
AUX_SOURCE_DIRECTORY(static_link_example DIR_STATIC)
add_executable(webapp ${DIR_STATIC})
