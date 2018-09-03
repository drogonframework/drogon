link_libraries(drogon trantor uuid pthread jsoncpp dl z)
if(OpenSSL_FOUND)
link_libraries(ssl crypto)
endif()
FILE(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/simple_example/*.csp)
foreach(cspFile ${SCP_LIST})
    message(STATUS "cspFile:" ${cspFile})
    EXEC_PROGRAM(basename ARGS "-s .csp ${cspFile}" OUTPUT_VARIABLE classname)
    message(STATUS "view classname:" ${classname})
    add_custom_command(OUTPUT ${classname}.h ${classname}.cc
        COMMAND drogon_ctl
        ARGS create view ${cspFile}
        DEPENDS ${cspFile}
        VERBATIM )
   set(VIEWSRC ${VIEWSRC} ${classname}.cc)
endforeach()


AUX_SOURCE_DIRECTORY(simple_example DIR_STATIC)

add_executable(webapp ${DIR_STATIC} ${VIEWSRC})
add_dependencies(webapp drogon_ctl)

AUX_SOURCE_DIRECTORY(client_example DIR_CLIENT)

add_executable(client ${DIR_CLIENT})

