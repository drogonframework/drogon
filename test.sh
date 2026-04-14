#!/usr/bin/env bash

echo "First arg:"
echo $1

os='linux'
if [ "X$1" = "X-w" ]; then
    os='windows'
fi
echo "OS:" $os

src_dir=$(pwd)
test_root=build/lib/tests
if [ "X$os" = "Xwindows" ]; then
    test_root=$test_root/Debug
fi

if [ "X$os" = "Xlinux" ]; then
    drogon_ctl_exec=$(pwd)/build/drogon_ctl/drogon_ctl
else
    drogon_ctl_exec=$(pwd)/build/drogon_ctl/Debug/drogon_ctl.exe
    export PATH=$PATH:$src_dir/install/bin
fi
echo "drogon_ctl_exec: " ${drogon_ctl_exec}

if [ "X$os" = "Xwindows" ]; then
    integration_test_client_exec=./integration_test_client.exe
    integration_test_server_exec=./integration_test_server.exe
else
    integration_test_client_exec=./integration_test_client
    integration_test_server_exec=./integration_test_server
fi

function update_config_line()
{
    local key="$1"
    local value="$2"
    local file="$3"
    sed -i.bak -e "s/\"${key}\".*$/\"${key}\": ${value},/" "$file"
    rm -f "$file.bak"
}

function cleanup_integration_test_server()
{
    if [ "X$os" = "Xwindows" ]; then
        taskkill //F //IM integration_test_server.exe > /dev/null 2>&1 || true
    else
        killall integration_test_server > /dev/null 2>&1 || true
        pkill -f '/integration_test_server$' > /dev/null 2>&1 || true
    fi
}

function wait_for_url()
{
    local url="$1"
    local timeout_seconds="$2"
    local curl_args=(--silent --show-error --output /dev/null --max-time 2)

    if [ "$url" != "${url#https://}" ]; then
        curl_args+=(--insecure)
    fi

    local attempt=0
    while [ $attempt -lt $timeout_seconds ]; do
        if curl "${curl_args[@]}" "$url"; then
            return 0
        fi

        attempt=$((attempt + 1))
        sleep 1
    done

    return 1
}

function wait_for_integration_test_server()
{
    local server_pid="$1"
    local server_log="$2"

    if ! wait_for_url "http://127.0.0.1:8848/" 30; then
        echo "Timed out waiting for integration_test_server to accept HTTP requests"
        if kill -0 "$server_pid" > /dev/null 2>&1; then
            echo "integration_test_server is still running, recent log output:"
        else
            echo "integration_test_server exited before becoming ready, recent log output:"
        fi
        if [ -f "$server_log" ]; then
            tail -n 50 "$server_log"
        fi
        return 1
    fi

    wait_for_url "https://127.0.0.1:8849/" 5 > /dev/null 2>&1 || true
    return 0
}

function get_cpu_count()
{
    if command -v nproc > /dev/null 2>&1; then
        nproc
        return
    fi

    if command -v getconf > /dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN
        return
    fi

    if command -v sysctl > /dev/null 2>&1; then
        sysctl -n hw.logicalcpu
        return
    fi

    echo 1
}

trap cleanup_integration_test_server EXIT

# Run the integration test server in the background and wait until it is ready.
function do_integration_test()
{
    pushd "$test_root"
    update_config_line "run_as_daemon" "false" config.example.json
    update_config_line "relaunch_on_error" "false" config.example.json
    update_config_line "number_of_threads" "0" config.example.json
    update_config_line "use_brotli" "true" config.example.json

    if [ "$1" = "stream_mode" ]; then
        update_config_line "enable_request_stream" "true" config.example.json
    else
        update_config_line "enable_request_stream" "false" config.example.json
    fi

    if [ ! -f "$integration_test_client_exec" ]; then
        echo "Build failed"
        exit -1
    fi
    if [ ! -f "$integration_test_server_exec" ]; then
        echo "Build failed"
        exit -1
    fi

    cleanup_integration_test_server

    local server_log=integration_test_server.log
    rm -f "$server_log"
    "$integration_test_server_exec" > "$server_log" 2>&1 &
    local server_pid=$!

    if ! wait_for_integration_test_server "$server_pid" "$server_log"; then
        exit -1
    fi

    echo "Running the integration test $1"
    "$integration_test_client_exec" -s

    if [ $? -ne 0 ]; then
        echo "Integration test failed $1"
        if [ -f "$server_log" ]; then
            tail -n 50 "$server_log"
        fi
        exit -1
    fi

    cleanup_integration_test_server
    popd
}

#Test drogon_ctl
function do_drogon_ctl_test()
{
    echo "Testing drogon_ctl"
    pushd "$test_root"
    rm -rf drogon_test

    ${drogon_ctl_exec} create project drogon_test

    ls -la
    cd drogon_test/controllers

    ${drogon_ctl_exec} create controller Test::SimpleCtrl
    ${drogon_ctl_exec} create controller -h Test::HttpCtrl
    ${drogon_ctl_exec} create controller -w Test::WebsockCtrl
    ${drogon_ctl_exec} create controller SimpleCtrl
    ${drogon_ctl_exec} create controller -h HttpCtrl
    ${drogon_ctl_exec} create controller -w WebsockCtrl

    if [ ! -f "Test_SimpleCtrl.h" -o ! -f "Test_SimpleCtrl.cc" -o ! -f "Test_HttpCtrl.h" -o ! -f "Test_HttpCtrl.cc" -o ! -f "Test_WebsockCtrl.h" -o ! -f "Test_WebsockCtrl.cc" ]; then
        echo "Failed to create controllers"
        exit -1
    fi

    if [ ! -f "SimpleCtrl.h" -o ! -f "SimpleCtrl.cc" -o ! -f "HttpCtrl.h" -o ! -f "HttpCtrl.cc" -o ! -f "WebsockCtrl.h" -o ! -f "WebsockCtrl.cc" ]; then
        echo "Failed to create controllers"
        exit -1
    fi

    cd ../filters

    ${drogon_ctl_exec} create filter Test::TestFilter

    if [ ! -f "Test_TestFilter.h" -o ! -f "Test_TestFilter.cc" ]; then
        echo "Failed to create filters"
        exit -1
    fi

    cd ../plugins

    ${drogon_ctl_exec} create plugin Test::TestPlugin

    if [ ! -f "Test_TestPlugin.h" -o ! -f "Test_TestPlugin.cc" ]; then
        echo "Failed to create plugins"
        exit -1
    fi

    cd ../views

    echo "Hello, world!" >>hello.csp

    cd ../build

    make_flags=''
    cmake_gen=''
    parallel=1
    cpu_count=$(get_cpu_count)

    # simulate ninja's parallelism
    case $cpu_count in
    1)
        parallel=$((cpu_count + 1))
        ;;
    2)
        parallel=$((cpu_count + 1))
        ;;
    *)
        parallel=$((cpu_count + 2))
        ;;
    esac

    if [ "X$os" = "Xlinux" ]; then
        if command -v ninja > /dev/null 2>&1; then
            cmake_gen='-G Ninja'
        else
            make_flags="$make_flags -j$parallel"
        fi
    else
        cmake_gen="$cmake_gen -DCMAKE_TOOLCHAIN_FILE=$src_dir/conan_toolchain.cmake \
                              -DCMAKE_PREFIX_PATH=$src_dir/install \
                              -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
                              -DCMAKE_CXX_STANDARD=17"
    fi
    cmake .. $cmake_gen

    if [ $? -ne 0 ]; then
        echo "Failed to run CMake for example project"
        exit -1
    fi

    cmake --build . -- $make_flags

    if [ $? -ne 0 ]; then
        echo "Error in testing"
        exit -1
    fi

        if [ "X$os" = "Xlinux" ]; then
            if [ ! -f "drogon_test" ]; then
                    echo "Failed to build drogon_test"
                    exit -1
            fi
    else
      if [ ! -f "Debug\drogon_test.exe" ]; then
          echo "Failed to build drogon_test"
          exit -1
      fi
    fi

    cd ../../
    rm -rf drogon_test
    popd
}

#unit testing
function do_unittest()
{
    echo "Unit testing"
    pushd "$src_dir/build"

    ctest . --output-on-failure
    if [ $? -ne 0 ]; then
        echo "Error in unit testing"
        exit -1
    fi
    popd
}

function do_db_test()
{
    pushd "$src_dir/build"
    if [ -f "./orm_lib/tests/db_test" ]; then
        echo "Test database"
        ./orm_lib/tests/db_test -s
        if [ $? -ne 0 ]; then
            echo "Error in testing"
            exit -1
        fi
    fi
    if [ -f "./orm_lib/tests/pipeline_test" ]; then
        echo "Test pipeline mode"
        ./orm_lib/tests/pipeline_test -s
        if [ $? -ne 0 ]; then
            echo "Error in testing"
            exit -1
        fi
    fi
    if [ -f "./orm_lib/tests/db_listener_test" ]; then
        echo "Test DbListener"
        ./orm_lib/tests/db_listener_test -s
        if [ $? -ne 0 ]; then
            echo "Error in testing"
            exit -1
        fi
    fi
    if [ -f "./orm_lib/tests/conn_options_test" ]; then
        echo "Test connection options"
        ./orm_lib/tests/conn_options_test -s
        if [ $? -ne 0 ]; then
            echo "Error in testing"
            exit -1
        fi
    fi
    if [ -f "./orm_lib/tests/db_api_test" ]; then
        echo "Test getDbClient() api"
        ./orm_lib/tests/db_api_test -s
        if [ $? -ne 0 ]; then
            echo "Error in testing"
            exit -1
        fi
    fi
    if [ -f "./nosql_lib/redis/tests/redis_test" ]; then
        echo "Test redis"
        ./nosql_lib/redis/tests/redis_test -s
        if [ $? -ne 0 ]; then
            echo "Error in testing"
            exit -1
        fi
    fi
}

if [ ! -f "$drogon_ctl_exec" ]
then
    echo "Warning: No built drogon_ctl, skip integration test and drogon_ctl test"
else
    do_integration_test
    do_integration_test stream_mode
    do_drogon_ctl_test
fi

if [ "X$1" = "X-t" ]; then
    do_unittest
    do_db_test
fi

echo "Everything is ok!"
exit 0
