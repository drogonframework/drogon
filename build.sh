#!/usr/bin/env bash

#building trantor
function build_trantor() {

    #Saving current directory
    current_dir="${PWD}"

    #The folder we will build
    build_dir='./build'
    if [ -d $build_dir ]; then
        echo "Deleted folder: ${build_dir}"
        rm -rf $build_dir
    fi

    #Creating building folder
    echo "Created building folder: ${build_dir}"
    mkdir $build_dir

    echo "Entering folder: ${build_dir}"
    cd $build_dir

    echo "Start building trantor ..."
    if [ $1 -eq 1 ]; then
        cmake .. -DBUILD_TESTING=on
    else
        cmake ..
    fi

    #If errors then exit
    if [ "$?" != "0" ]; then
        exit -1
    fi

    make

    #If errors then exit
    if [ "$?" != "0" ]; then
        exit -1
    fi

    echo "Installing ..."
    sudo make install

    #Reback current directory
    cd $current_dir
    exit 0
    #Ok!
}

if [ "$1" = "-t" ]; then
    build_trantor 1
else
    build_trantor 0
fi
