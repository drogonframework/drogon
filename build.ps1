# This script is used for windows only,
# if you are using linux/macOS, please use build.sh instead of it.
# conan only provide all x64 dependencies, so all test need baseded on x64.

param(
    [string]$BUILD_PARA = '',
    [string]$COMPILER = 'Visual Studio',
    [Int]$COMPILER_VERSION = 16
)

$OS=$env:OS
Write-Host -ForegroundColor:Magenta "OS:$OS"
if ($OS -ne "Windows_NT"){
	Write-Host -ForegroundColor:Red "This script is for windows, please use build.sh for other OS"
	exit -1
}
Write-Host -ForegroundColor:Magenta "BUILD_PARA:$BUILD_PARA"
Write-Host -ForegroundColor:Magenta "COMPILER:$COMPILER"
Write-Host -ForegroundColor:Magenta "COMPILER_VERSION:$COMPILER_VERSION"

Write-Host -ForegroundColor:Green '
# Using params for this script as bellow:
# no arg: no test, release
# arg "t": with test, debug
# arg "tshared": with test, shared, debug
build.ps1 t
build.ps1 tshared
build.ps1 -COMPILER "Visual Studio" -COMPILER_VERSION 16
build.ps1 -BUILD_PARA t -COMPILER "Visual Studio" -COMPILER_VERSION 16
build.ps1 -BUILD_PARA tshared -COMPILER "Visual Studio" -COMPILER_VERSION 16
 '

$src_dir=$PWD
# set arch to x64, if compiler is Visual Studio, due to conan package manager only provide x64 packages currently.
if ("$COMPILER" -eq "Visual Studio") {
    Write-Host -ForegroundColor:Magenta "Setting Visual Studio to arch x64"
    if (Test-Path -Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise") {
        $VsInstallPath="C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise"
    } elseif (Test-Path -Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional"){
        $VsInstallPath="C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional"
    } elseif (Test-Path -Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community"){
        $VsInstallPath="C:\Program Files (x86)\Microsoft Visual Studio\2022\Community"
    } elseif (Test-Path -Path "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise") {
        $VsInstallPath="C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise"
    } elseif (Test-Path -Path "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional"){
        $VsInstallPath="C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional"
    } elseif (Test-Path -Path "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"){
        $VsInstallPath="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
    } else {
        Write-Host -ForegroundColor:Red "Error in found Visual Studio install path"
        exit -1
    }
    Import-Module ("$VsInstallPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
    Enter-VsDevShell -VsInstallPath "$VsInstallPath" -DevCmdArguments "-arch=x64 -host_arch=x64" -SkipAutomaticLocation
} else {
    Write-Host -ForegroundColor:Yellow "Suggest you using Visual Studio and conan package management."
    exit -1
}

# build drogon
function build_drogon($para){
    # Update the submodule and initialize
    git submodule update --init

    # Remove the config.h generated by the old version of drogon.
    Remove-item  ./lib/inc/drogon/config.h -ErrorAction SilentlyContinue

    # Save current directory
    $current_dir=$PWD

    # The folder in which we will build drogon
    $build_dir="$current_dir/build"
    if (Test-Path -Path $build_dir) {
        echo "Deleted folder: $build_dir"
        Remove-item $build_dir -Force -Recurse -ErrorAction SilentlyContinue
    }

    # Create building and install folder
    echo "Created building and install folder: $build_dir"
    mkdir -p $build_dir/install

    echo "Entering folder: $build_dir"
    cd $build_dir

    # build
    echo "Start building drogon ..."
    if ( $para -eq 1 ) {
        conan install .. -s compiler=$COMPILER -s compiler.version=$COMPILER_VERSION -s build_type=Debug -g cmake_paths
        cmake .. -DBUILD_TESTING=YES -DCMAKE_INSTALL_PREFIX="install" -DCMAKE_TOOLCHAIN_FILE="conan_paths.cmake" $ignore_lnk4099.Split() $cmake_gen
    } elseif ( $para -eq 2 ) {
        conan install .. -s compiler=$COMPILER -s compiler.version=$COMPILER_VERSION -s build_type=Debug -g cmake_paths
        cmake .. -DBUILD_TESTING=YES -DBUILD_DROGON_SHARED=ON -DCMAKE_CXX_VISIBILITY_PRESET=hidden -DCMAKE_VISIBILITY_INLINES_HIDDEN=1 -DCMAKE_INSTALL_PREFIX="install" -DCMAKE_TOOLCHAIN_FILE="conan_paths.cmake"  $ignore_lnk4099.Split() $cmake_gen
    } else {
        conan install .. -s compiler=$COMPILER -s compiler.version=$COMPILER_VERSION -s build_type=Release -g cmake_paths
        cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$build_dir/conan_paths.cmake" $cmake_gen
        # cmake using '--config Release' for MSVC
        if ("$make_program" -eq "cmake") {
           $make_flags=$make_flags+ ' --config Release'
        }
    }
    & $make_program $make_flags.Split(" ")

    # If errors then exit
    if ( !$? ) {
        exit -1
    }

    # Go back to the current directory
    cd $current_dir
    # Ok!
}

$ignore_lnk4099='-DCMAKE_EXE_LINKER_FLAGS="/ignore:4099" -DCMAKE_SHARED_LINKER_FLAGS="/ignore:4099" -DCMAKE_STATIC_LINKER_FLAGS="/ignore:4099"'
$make_program='cmake'
$cmake_gen=''
$make_flags='--build . --target install --parallel'

if ((where.exe "Ninja")) {
    $make_program='Ninja'
	$cmake_gen='-GNinja'
    $make_flags='install'
}

if ($BUILD_PARA -eq "t") {
    build_drogon 1
} elseif ($BUILD_PARA -eq "tshared") {
    build_drogon 2
} else {
    build_drogon 0
}