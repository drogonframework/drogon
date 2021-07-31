set(ctl_sources
    cmd.cc
    create.cc
    create_controller.cc
    create_filter.cc
    create_model.cc
    create_plugin.cc
    create_project.cc
    create_view.cc
    create_swagger.cc
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
if (WIN32 AND BUILD_DROGON_SHARED)
  set(DROGON_FILE $<TARGET_FILE:drogon>)
  set(TRANTOR_FILE $<TARGET_FILE:trantor>)
  add_custom_command(TARGET _drogon_ctl POST_BUILD
          COMMAND ${CMAKE_COMMAND}
          -DCTL_FILE=${DROGON_FILE}
          -DINSTALL_BIN_DIR=$<TARGET_FILE_DIR:_drogon_ctl>
          -P
          ${CMAKE_CURRENT_SOURCE_DIR}/CopyDlls.cmake)
  add_custom_command(TARGET _drogon_ctl POST_BUILD
          COMMAND ${CMAKE_COMMAND}
          -DCTL_FILE=${TRANTOR_FILE}
          -DINSTALL_BIN_DIR=$<TARGET_FILE_DIR:_drogon_ctl>
          -P
          ${CMAKE_CURRENT_SOURCE_DIR}/CopyDlls.cmake)
endif()
file(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/templates/*.csp)
foreach(cspFile ${SCP_LIST})
  message(STATUS "cspFile:" ${cspFile})
  get_filename_component(classname ${cspFile} NAME_WE)
  message(STATUS "view classname:" ${classname})
  add_custom_command(OUTPUT ${classname}.h ${classname}.cc
                     COMMAND $<TARGET_FILE:_drogon_ctl>
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
add_dependencies(drogon_ctl _drogon_ctl)
if(WIN32)
  target_link_libraries(drogon_ctl PRIVATE ws2_32 Rpcrt4)
endif(WIN32)
if(APPLE)
  target_link_libraries(drogon_ctl PRIVATE resolv)
endif()
message(STATUS "bin:" ${INSTALL_BIN_DIR})
install(TARGETS drogon_ctl RUNTIME DESTINATION ${INSTALL_BIN_DIR})
if(WIN32)
  set(CTL_FILE $<TARGET_FILE:drogon_ctl>)
  add_custom_command(TARGET drogon_ctl POST_BUILD
                     COMMAND ${CMAKE_COMMAND}
                             -DCTL_FILE=${CTL_FILE}
                             -DINSTALL_BIN_DIR=${INSTALL_BIN_DIR}
                             -DRENAME_EXE=ON
                             -P
                             ${CMAKE_CURRENT_SOURCE_DIR}/CopyDlls.cmake)
else(WIN32)
  install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ./drogon_ctl \
    ./dg_ctl \
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dg_ctl"
          DESTINATION ${INSTALL_BIN_DIR})
endif(WIN32)
find_package(Clang REQUIRED)
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)

# Link against LLVM libraries
target_link_libraries(drogon_ctl PRIVATE ${llvm_libs})

set(ctl_targets _drogon_ctl drogon_ctl)
set_property(TARGET ${ctl_targets} PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${ctl_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${ctl_targets} PROPERTY CXX_EXTENSIONS OFF)
