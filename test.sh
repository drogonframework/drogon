#!/bin/bash
cd build/examples/

#Make webapp run as a daemon
sed -i -e "s/\"run_as_daemon.*$/\"run_as_daemon\": true\,/" config.example.json
sed -i -e "s/\"relaunch_on_error.*$/\"relaunch_on_error\": true\,/" config.example.json

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

if [ $? -ne 0 ];then
    echo "Error in testing"
    exit -1
fi

#Test WebSocket
echo "Test the WebSocket"
./websocket_test -t
if [ $? -ne 0 ];then
    echo "Error in testing"
    exit -1
fi

#Test pipelining
echo "Test the pipelining"
./pipelining_test
if [ $? -ne 0 ];then
    echo "Error in testing"
    exit -1
fi

killall -9 webapp

#Test drogon_ctl
echo "Test the drogon_ctl"
rm -rf drogon_test

drogon_ctl create project drogon_test

cd drogon_test/controllers

drogon_ctl create controller Test::SimpleCtrl
drogon_ctl create controller -h Test::HttpCtrl
drogon_ctl create controller -w Test::WebsockCtrl

if [ ! -f "Test_SimpleCtrl.h" -o ! -f "Test_SimpleCtrl.cc" -o ! -f "Test_HttpCtrl.h" -o ! -f "Test_HttpCtrl.cc" -o ! -f "Test_WebsockCtrl.h" -o ! -f "Test_WebsockCtrl.cc" ];then
    echo "Failed to create controllers"
    exit -1
fi

cd ../filters

drogon_ctl create filter Test::TestFilter

if [ ! -f "Test_TestFilter.h" -o ! -f "Test_TestFilter.cc" ];then
    echo "Failed to create filters"
    exit -1
fi

cd ../build
cmake ..

if [ $? -ne 0 ];then
    echo "Error in testing"
    exit -1
fi

make

if [ $? -ne 0 ];then
    echo "Error in testing"
    exit -1
fi

if [ ! -f "drogon_test" ];then
    echo "Failed to build drogon_test"
    exit -1
fi

cd ../../
rm -rf drogon_test

echo "Everything is ok!"
exit 0
