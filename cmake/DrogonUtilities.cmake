# ##############################################################################
# function drogon_create_views(target source_path output_path
# [use_path_as_namespace])
# ##############################################################################
function(drogon_create_views arg)
  if(ARGC LESS 3)
    message(STATUS "arguments error when calling drogon_create_views")
    return()
  endif()
  file(MAKE_DIRECTORY ${ARGV2})
  file(GLOB_RECURSE SCP_LIST ${ARGV1}/*.csp)
  foreach(cspFile ${SCP_LIST})
    file(RELATIVE_PATH
         inFile
         ${CMAKE_CURRENT_SOURCE_DIR}
         ${cspFile})
    if(ARGC GREATER 3 AND ARGV3)
      string(REPLACE "/"
                     "_"
                     f1
                     ${inFile})
      string(REPLACE "\\"
                     "_"
                     f2
                     ${f1})
      string(REPLACE ".csp"
                     ""
                     outputFile
                     ${f2})
      add_custom_command(OUTPUT ${ARGV2}/${outputFile}.h ${ARGV2}/${outputFile}.cc
                         COMMAND drogon_ctl
                                 ARGS
                                 create
                                 view
                                 ${inFile}
                                 --path-to-namespace
                                 -o
                                 ${ARGV2}
                         DEPENDS ${cspFile}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                         VERBATIM)
      set(VIEWSRC ${VIEWSRC} ${ARGV2}/${outputFile}.cc)
    else()
      get_filename_component(classname ${cspFile} NAME_WE)
      add_custom_command(OUTPUT ${ARGV2}/${classname}.h ${ARGV2}/${classname}.cc
                         COMMAND drogon_ctl
                                 ARGS
                                 create
                                 view
                                 ${inFile}
                                 -o
                                 ${ARGV2}
                         DEPENDS ${cspFile}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                         VERBATIM)
      set(VIEWSRC ${VIEWSRC} ${ARGV2}/${classname}.cc)
    endif()
  endforeach()
  target_sources(${ARGV0} PRIVATE ${VIEWSRC})
endfunction(drogon_create_views)
