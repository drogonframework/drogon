#!/bin/bash

#building drogon
function build_drogon() {

    #update the submodule and initialize
    git submodule update --init
    
    #saving current directory
    current_dir="${PWD}"

    #the folder we will build
    build_dir='./build'
    if [ -d $build_dir ]; then
        echo "Deleted folder: ${build_dir}"
        rm -rf $build_dir
    fi

    #creating building folder
    echo "Created building folder: ${build_dir}"
    mkdir $build_dir

    echo "Entering folder: ${build_dir}"
    cd $build_dir

    echo "Start building drogon ..."
    cmake ..
    
    #errors exit
    if [ "$?" != "0" ]; then
        exit
    fi
    
    make
    
    #errors exit
    if [ "$?" != "0" ]; then
        exit
    fi

    echo "Installing header files ..."
    sudo make install

    #reback current directory
    cd $current_dir
    #ok!
}

build_drogon
