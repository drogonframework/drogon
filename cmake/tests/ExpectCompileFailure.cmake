if (NOT DEFINED BUILD_DIR OR NOT DEFINED TARGET_NAME OR
    NOT DEFINED EXPECTED_DIAGNOSTIC)
    message(STATUS
        "BUILD_DIR, TARGET_NAME, and EXPECTED_DIAGNOSTIC are required")
    return()
endif ()

set(build_command
    "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${TARGET_NAME}")
if (DEFINED BUILD_CONFIG AND NOT BUILD_CONFIG STREQUAL "")
    list(APPEND build_command --config "${BUILD_CONFIG}")
endif ()

execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr)

set(build_output "${build_stdout}\n${build_stderr}")

if (build_result EQUAL 0)
    message(STATUS
        "${TARGET_NAME} unexpectedly compiled successfully")
    return()
endif ()

if (NOT build_output MATCHES "${EXPECTED_DIAGNOSTIC}")
    message(STATUS
        "${TARGET_NAME} failed without the expected diagnostic "
        "'${EXPECTED_DIAGNOSTIC}':\n${build_output}")
    return()
endif ()

# The CTest entry uses WILL_FAIL. Reaching this fatal error means the selected
# target failed to compile for the reason that the test expected.
message(FATAL_ERROR
    "Expected compile failure observed for ${TARGET_NAME}: "
    "${EXPECTED_DIAGNOSTIC}")
