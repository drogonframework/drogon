link_libraries(drogon trantor uuid pthread jsoncpp dl z)
if(OpenSSL_FOUND)
link_libraries(ssl crypto)
endif()

AUX_SOURCE_DIRECTORY(. SRC_DIR)
add_executable(_drogon_ctl main.cc cmd.cc create.cc create_view.cc)
FILE(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/templates/*.csp)
foreach(cspFile ${SCP_LIST})
    message(STATUS "cspFile:" ${cspFile})
    EXEC_PROGRAM(basename ARGS "${cspFile} .csp" OUTPUT_VARIABLE classname)
    message(STATUS "view classname:" ${classname})
    add_custom_command(OUTPUT ${classname}.h ${classname}.cc
        COMMAND _drogon_ctl
        ARGS create view ${cspFile}
        DEPENDS ${cspFile}
        VERBATIM )
   set(TEMPL_SRC ${TEMPL_SRC} ${classname}.cc)
endforeach()
add_executable(drogon_ctl ${SRC_DIR} ${TEMPL_SRC})
add_dependencies(drogon_ctl trantor makeVersion _drogon_ctl)
install(TARGETS drogon_ctl DESTINATION bin)
