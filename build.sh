#!/bin/bash

#building drogon
function build_drogon() {

    #Update the submodule and initialize
    git submodule update --init
    
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

    #Reback current directory
    cd $current_dir
    #Ok!
}

build_drogon
