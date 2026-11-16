cmake_minimum_required(VERSION 3.12...3.31)
project(model_generation_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard for generated models")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Drogon CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(DROGON_CTL drogon_ctl)
if(NOT DROGON_CTL)
    message(FATAL_ERROR "Set DROGON_CTL to the installed drogon_ctl executable")
endif()

set(model_dir "${CMAKE_CURRENT_BINARY_DIR}/models")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate.py"
            "${DROGON_CTL}" "${model_dir}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Model generation failed: ${result}")
endif()
add_executable(model_generation_test main.cc
    "${model_dir}/NoKey.cc" "${model_dir}/CompositeKey.cc"
    "${model_dir}/ManualKey.cc" "${model_dir}/AutoKey.cc")
target_include_directories(model_generation_test PRIVATE "${model_dir}")
target_link_libraries(model_generation_test PRIVATE Drogon::Drogon)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(model_generation_test PRIVATE -Werror=unused-parameter)
elseif(MSVC)
    target_compile_options(model_generation_test PRIVATE /w14100 /we4100)
endif()
enable_testing()
add_test(NAME generated_model_keys COMMAND model_generation_test "${model_dir}/models.db")
