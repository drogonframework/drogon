#==================================================================================================#
# Adapted and re-written from Catch2 to work with Drogon Test                                      #
#                                                                                                  #
#  Usage                                                                                           #
# 1. make sure this module is in the path or add this otherwise:                                   #
#    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")              #
# 2. make sure that you've enabled testing option for the project by the call:                     #
#    enable_testing()                                                                              #
# 3. add the lines to the script for testing target (sample CMakeLists.txt):                       #
#        project(testing_target)                                                                   #
#        enable_testing()                                                                          #
#                                                                                                  #
#        file(GLOB SOURCE_FILES "*.cpp")                                                           #
#        add_executable(${PROJECT_NAME} ${SOURCE_FILES})                                           #
#                                                                                                  #
#        include(ParseAndAddDrogonTests)                                                          #
#        ParseAndAddDrogonTests(${PROJECT_NAME})                                                   #
#==================================================================================================#

cmake_minimum_required(VERSION 3.5)

# This removes the contents between
#  - block comments (i.e. /* ... */) 
#  - full line comments (i.e. // ... ) 
# contents have been read into '${CppCode}'.
# !keep partial line comments
function(RemoveComments CppCode)
  string(ASCII 2 CMakeBeginBlockComment)
  string(ASCII 3 CMakeEndBlockComment)
  string(REGEX REPLACE "/\\*" "${CMakeBeginBlockComment}" ${CppCode} "${${CppCode}}")
  string(REGEX REPLACE "\\*/" "${CMakeEndBlockComment}" ${CppCode} "${${CppCode}}")
  string(REGEX REPLACE "${CMakeBeginBlockComment}[^${CMakeEndBlockComment}]*${CMakeEndBlockComment}" "" ${CppCode} "${${CppCode}}")
  string(REGEX REPLACE "\n[ \t]*//+[^\n]+" "\n" ${CppCode} "${${CppCode}}")

  set(${CppCode} "${${CppCode}}" PARENT_SCOPE)
endfunction()

# Worker function
function(ParseFile SourceFile TestTarget)
	set(FullSourcePath ${CMAKE_CURRENT_SOURCE_DIR}/${SourceFile})
    if(NOT EXISTS ${FullSourcePath})
        return()
    endif()
    file(STRINGS ${FullSourcePath} Contents NEWLINE_CONSUME)

    # Remove block and fullline comments
    RemoveComments(Contents)

    # Find definition of test names
    string(REGEX MATCHALL "[ \t]*DROGON_TEST[ \t]*\\\([a-zA-Z0-9_]+\\\)" Tests "${Contents}")

    foreach(TestLine ${Tests})
        # Strip newlines
        string(REGEX REPLACE "\\\\\n|\n" "" TestLine "${TestLine}")

        # Get the name of the test
		string(REGEX REPLACE "[ \t]*DROGON_TEST[ \t]*" "" TestLine "${TestLine}")
        string(REGEX MATCHALL "[a-zA-Z0-9_]+" TestName "${TestLine}")

        # Validate that a test name and tags have been provided
        list(LENGTH TestName TestNameLength)
        if(NOT TestNameLength EQUAL 1)
            message(FATAL_ERROR "${TestName} in ${SourceFile} is not a valid test name."
				" Either a bug in the Drogon Test CMake parser or a bug in the test itself")
        endif()

        # Add the test and set its properties
        add_test(NAME "${TestName}" COMMAND ${TestTarget} -r ${TestName} ${AdditionalCatchParameters})

    endforeach()
endfunction()

# entry point
function(ParseAndAddDrogonTests TestTarget)
    get_target_property(SourceFiles ${TestTarget} SOURCES)
    foreach(SourceFile ${SourceFiles})
        ParseFile(${SourceFile} ${TestTarget})
    endforeach()
endfunction()