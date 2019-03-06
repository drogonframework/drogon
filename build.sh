#!/bin/bash

#build drogon
function build_drogon() {

    #Update the submodule and initialize
    git submodule update --init
    
    #Save current directory
    current_dir="${PWD}"

    #The folder in which we will build drogon
    build_dir='./build'
    if [ -d $build_dir ]; then
        echo "Deleted folder: ${build_dir}"
        rm -rf $build_dir
    fi

    #Create building folder
    echo "Created building folder: ${build_dir}"
    mkdir $build_dir

    echo "Entering folder: ${build_dir}"
    cd $build_dir

    echo "Start building drogon ..."
    cmake ..
    
    #If errors then exit
    if [ "$?" != "0" ]; then
        exit
    fi
    
    make
    
    #If errors then exit
    if [ "$?" != "0" ]; then
        exit
    fi

    echo "Installing ..."
    sudo make install

    #Go back to the current directory
    cd $current_dir
    #Ok!
}

build_drogon
