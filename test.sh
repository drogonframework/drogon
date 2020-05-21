#!/usr/bin/env bash

drogon_ctl_exec=`pwd`/build/drogon_ctl/drogon_ctl
echo ${drogon_ctl_exec}
cd build/examples/

make_program=make
make_flags=''
cmake_gen=''
parallel=1

# simulate ninja's parallelism
case $(nproc) in
 1)
    parallel=$(( $(nproc) + 1 ))
    ;;
 2)
    parallel=$(( $(nproc) + 1 ))
    ;;
 *)
    parallel=$(( $(nproc) + 2 ))
    ;;
esac

if [ -f /bin/ninja ]; then
    make_program=ninja
    cmake_gen='-G Ninja'
else
    make_flags="$make_flags -j$parallel"
fi

#Make webapp run as a daemon
sed -i -e "s/\"run_as_daemon.*$/\"run_as_daemon\": true\,/" config.example.json
sed -i -e "s/\"relaunch_on_error.*$/\"relaunch_on_error\": true\,/" config.example.json
sed -i -e "s/\"threads_num.*$/\"threads_num\": 0\,/" config.example.json
sed -i -e "s/\"use_brotli.*$/\"use_brotli\": true\,/" config.example.json

if [ ! -f "webapp" ]; then
    echo "Build failed"
    exit -1
fi
if [ ! -f "webapp_test" ]; then
    echo "Build failed"
    exit -1
fi

killall -9 webapp
./webapp

sleep 4

echo "Test http requests and responses."
./webapp_test

if [ $? -ne 0 ]; then
    echo "Error in testing http requests"
    exit -1
fi

#Test WebSocket
echo "Test the WebSocket"
./websocket_test -t
if [ $? -ne 0 ]; then
    echo "Error in testing WebSocket"
    exit -1
fi

#Test pipelining
echo "Test the pipelining"
./pipelining_test
if [ $? -ne 0 ]; then
    echo "Error in testing pipelining"
    exit -1
fi

killall -9 webapp

#Test drogon_ctl
echo "Test the drogon_ctl"
rm -rf drogon_test

${drogon_ctl_exec} create project drogon_test

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

cd ../build
cmake .. $cmake_gen

if [ $? -ne 0 ]; then
    echo "Error in testing"
    exit -1
fi

$make_program $make_flags

if [ $? -ne 0 ]; then
    echo "Error in testing"
    exit -1
fi

if [ ! -f "drogon_test" ]; then
    echo "Failed to build drogon_test"
    exit -1
fi

cd ../../
rm -rf drogon_test

if [ "$1" = "-t" ]; then
    #unit testing
    cd ../
    echo "Unit testing"
    $make_program $make_flags test
    if [ $? -ne 0 ]; then
        echo "Error in unit testing"
        exit -1
    fi
    echo "Test database"
    ./orm_lib/tests/db_test
    if [ $? -ne 0 ]; then
        echo "Error in testing"
        exit -1
    fi
fi

echo "Everything is ok!"
exit 0
