link_libraries(drogon)
AUX_SOURCE_DIRECTORY(. SRC_DIR)
add_executable(drogon_ctl ${SRC_DIR})
install(TARGETS drogon_ctl DESTINATION bin)
