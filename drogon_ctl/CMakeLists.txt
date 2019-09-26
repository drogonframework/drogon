set(ctl_sources
    cmd.cc
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
add_executable(_drogon_ctl
               main.cc
               cmd.cc
               create.cc
               create_view.cc)
target_link_libraries(_drogon_ctl ${PROJECT_NAME})
file(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/templates/*.csp)
foreach(cspFile ${SCP_LIST})
  message(STATUS "cspFile:" ${cspFile})
  exec_program(basename
               ARGS
               "${cspFile} .csp"
               OUTPUT_VARIABLE
               classname)
  message(STATUS "view classname:" ${classname})
  add_custom_command(OUTPUT ${classname}.h ${classname}.cc
                     COMMAND _drogon_ctl
                             ARGS
                             create
                             view
                             ${cspFile}
                     DEPENDS ${cspFile}
                     VERBATIM)
  set(TEMPL_SRC ${TEMPL_SRC} ${classname}.cc)
endforeach()
add_executable(drogon_ctl ${ctl_sources} ${TEMPL_SRC})
target_link_libraries(drogon_ctl PRIVATE ${PROJECT_NAME})
target_include_directories(drogon_ctl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(drogon_ctl
                 trantor
                 makeVersion
                 _drogon_ctl)
message(STATUS "bin:" ${INSTALL_BIN_DIR})
install(TARGETS drogon_ctl RUNTIME DESTINATION ${INSTALL_BIN_DIR})
install(PROGRAMS $<TARGET_FILE_DIR:drogon_ctl>/drogon_ctl
        DESTINATION ${INSTALL_BIN_DIR}
        RENAME dg_ctl)
set(ctl_targets _drogon_ctl drogon_ctl)
set_property(TARGET ${ctl_targets} PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${ctl_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${ctl_targets} PROPERTY CXX_EXTENSIONS OFF)
