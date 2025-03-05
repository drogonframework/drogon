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

#Make integration_test_server run as a daemon
function do_integration_test()
{
    pushd $test_root
    if [ "X$os" = "Xlinux" ]; then
        sed -i -e "s/\"run_as_daemon.*$/\"run_as_daemon\": true\,/" config.example.json
    fi
    sed -i -e "s/\"relaunch_on_error.*$/\"relaunch_on_error\": true\,/" config.example.json
    sed -i -e "s/\"threads_num.*$/\"threads_num\": 0\,/" config.example.json
    sed -i -e "s/\"use_brotli.*$/\"use_brotli\": true\,/" config.example.json

    if [ "$1" = "stream_mode" ]; then
        sed -i -e "s/\"enable_request_stream.*$/\"enable_request_stream\": true\,/" config.example.json
    else
        sed -i -e "s/\"enable_request_stream.*$/\"enable_request_stream\": false\,/" config.example.json
    fi

    if [ ! -f "integration_test_client" ]; then
        echo "Build failed"
        exit -1
    fi
    if [ ! -f "integration_test_server" ]; then
        echo "Build failed"
        exit -1
    fi

    killall -9 integration_test_server
    ./integration_test_server &

    sleep 4

    echo "Running the integration test $1"
    ./integration_test_client -s

    if [ $? -ne 0 ]; then
        echo "Integration test failed $1"
        exit -1
    fi

    killall -9 integration_test_server
    popd
}

#Test drogon_ctl
function do_drogon_ctl_test()
{
    echo "Testing drogon_ctl"
    pushd $test_root
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

    # simulate ninja's parallelism
    case $(nproc) in
    1)
        parallel=$(($(nproc) + 1))
        ;;
    2)
        parallel=$(($(nproc) + 1))
        ;;
    *)
        parallel=$(($(nproc) + 2))
        ;;
    esac

    if [ "X$os" = "Xlinux" ]; then
        if [ -f /bin/ninja ]; then
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
    pushd $src_dir/build

    ctest . --output-on-failure
    if [ $? -ne 0 ]; then
        echo "Error in unit testing"
        exit -1
    fi
    popd
}

function do_db_test()
{
    pushd $src_dir/build
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

if ! drogon_ctl -v > /dev/null 2>&1
then
    echo "Warning: No drogon_ctl, skip integration test and drogon_ctl test"
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
