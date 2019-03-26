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
./webapp_test

if [ $? -ne 0 ];then
    echo "Error in testing"
    exit -1
fi

exit 0
