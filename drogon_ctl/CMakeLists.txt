LINK_LIBRARIES(drogon trantor pthread dl)

SET(ctl_sources cmd.cc
                create.cc
                create_controller.cc
                create_filter.cc
                create_model.cc
                create_plugin.cc
                create_project.cc
                create_view.cc
                help.cc
                main.cc
                press.cc
                version.cc)
ADD_EXECUTABLE(_drogon_ctl main.cc cmd.cc create.cc create_view.cc)
FILE(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/templates/*.csp)
FOREACH(cspFile ${SCP_LIST})
    MESSAGE(STATUS "cspFile:" ${cspFile})
    EXEC_PROGRAM(basename ARGS "${cspFile} .csp" OUTPUT_VARIABLE classname)
    MESSAGE(STATUS "view classname:" ${classname})
    ADD_CUSTOM_COMMAND(OUTPUT ${classname}.h ${classname}.cc
        COMMAND _drogon_ctl
        ARGS create view ${cspFile}
        DEPENDS ${cspFile}
        VERBATIM )
   SET(TEMPL_SRC ${TEMPL_SRC} ${classname}.cc)
ENDFOREACH()
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
ADD_EXECUTABLE(drogon_ctl ${ctl_sources} ${TEMPL_SRC})
ADD_DEPENDENCIES(drogon_ctl trantor makeVersion _drogon_ctl)
INSTALL(TARGETS drogon_ctl DESTINATION bin)
INSTALL(PROGRAMS $<TARGET_FILE_DIR:drogon_ctl>/drogon_ctl DESTINATION bin RENAME dg_ctl)
SET(ctl_targets _drogon_ctl drogon_ctl)
SET_PROPERTY(TARGET ${ctl_targets} PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
SET_PROPERTY(TARGET ${ctl_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
SET_PROPERTY(TARGET ${ctl_targets} PROPERTY CXX_EXTENSIONS OFF)

