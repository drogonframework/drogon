#!/usr/bin/env bash

#build drogon
function build_drogon() {

    #Update the submodule and initialize
    git submodule update --init
    
    #Remove the config.h generated by the old version of drogon.
    rm -f lib/inc/drogon/config.h

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
    if [ $1 -eq 1 ]; then
        cmake .. -DBUILD_TESTING=YES $cmake_gen
    elif [ $1 -eq 2 ]; then
        cmake .. -DBUILD_TESTING=YES -DBUILD_DROGON_SHARED=ON -DCMAKE_CXX_VISIBILITY_PRESET=hidden -DCMAKE_VISIBILITY_INLINES_HIDDEN=1 $cmake_gen
    else
        cmake .. -DCMAKE_BUILD_TYPE=release $cmake_gen
    fi

    #If errors then exit
    if [ "$?" != "0" ]; then
        exit -1
    fi
    
    $make_program $make_flags
    
    #If errors then exit
    if [ "$?" != "0" ]; then
        exit -1
    fi

    echo "Installing ..."
    $make_program install

    #Go back to the current directory
    cd $current_dir
    #Ok!
}

make_program=make
make_flags=''
cmake_gen=''
parallel=1

case $(uname) in
 FreeBSD)
    nproc=$(sysctl -n hw.ncpu)
    ;;
 Darwin)
    nproc=$(sysctl -n hw.ncpu) # sysctl -n hw.ncpu is the equivalent to nproc on macOS.
    ;;
 *)
    nproc=$(nproc)
    ;;
esac

# simulate ninja's parallelism
case nproc in
 1)
    parallel=$(( nproc + 1 ))
    ;;
 2)
    parallel=$(( nproc + 1 ))
    ;;
 *)
    parallel=$(( nproc + 2 ))
    ;;
esac

if [ -f /bin/clang++ ]; then
    cmake_gen="$cmake_gen -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++"
else
    cmake_gen="$cmake_gen -D CMAKE_C_COMPILER=gcc -D CMAKE_CXX_COMPILER=g++"
fi

if [ -f /bin/ninja ]; then
    make_program=ninja
    cmake_gen="$cmake_gen -GNinja"
else
    make_flags="$make_flags -j$parallel"
fi

if [ "$1" = "-t" ]; then
    build_drogon 1
elif [ "$1" = "-tshared" ]; then
    build_drogon 2
else
    build_drogon 0
fi
