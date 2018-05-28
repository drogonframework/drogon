#link_libraries(drogon trantor uuid pthread jsoncpp)
AUX_SOURCE_DIRECTORY(. SRC_DIR)
add_executable(drogon_ctl ${SRC_DIR})
install(TARGETS drogon_ctl DESTINATION bin)
