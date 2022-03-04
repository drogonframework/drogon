# This script is used for windows only,
# if you are using linux/macOS, please use test.sh instead of it.
# conan only provide all x64 dependencies, so all test need baseded on x64.

param(
    [string]$BUILD_TYPE = 'Debug',
    [string]$COMPILER = 'Visual Studio',
    [Int]$COMPILER_VERSION = 16
)

Write-Host -ForegroundColor:Magenta "BUILD_TYPE:$BUILD_TYPE"
Write-Host -ForegroundColor:Magenta "VS COMPILER_VERSION:$COMPILER_VERSION"
$OS=$env:OS
Write-Host -ForegroundColor:Magenta "OS:$OS"
if ($OS -ne "Windows_NT"){
	Write-Host -ForegroundColor:Red "This script is for windows, please use test.sh for other OS"
	exit -1
}

Write-Host -ForegroundColor:Green '
# Install conan for Visual Studio first, download dependencies(only support x64), then build project as bellow:
mkdir build
cd build
conan install .. -s compiler="Visual Studio" -s compiler.version=16 -s build_type=Debug -g cmake_paths
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_DROGON_SHARED=ON -DBUILD_CTL=ON -DBUILD_EXAMPLES=ON -DCMAKE_INSTALL_PREFIX=$ABSOLUTE_PATH/install -DCMAKE_TOOLCHAIN_FILE=$ABSOLUTE_PATH/conan_paths.cmake
cmake --build . --parallel --target install

# Using params for this script as bellow: 
test.ps1 Debug 16
test.ps1 -BUILD_TYPE Debug -COMPILER "Visual Studio" -COMPILER_VERSION 16
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
    Write-Host -ForegroundColor:Magenta "Please confirm all dependencies have been installed."
}

$cmake_gen=''
$cmake_test_target='test'
$parallel=$env:NUMBER_OF_PROCESSORS

if ( "1" -eq (Select-String "CMAKE_GENERATOR:INTERNAL=Ninja" $src_dir/build/CMakeCache.txt).count ) {
    $cmake_gen='-G Ninja'
} elseif ( "1" -eq (Select-String "CMAKE_GENERATOR:INTERNAL=Visual Studio" $src_dir/build/CMakeCache.txt).count ) {
    $cmake_test_target='RUN_TESTS'
} else {}

# Unit testing
cd $src_dir/build
Write-Host -ForegroundColor:Cyan "Unit testing"
cmake --build .  --target "$cmake_test_target"

if ( !$? ) {
    Write-Host -ForegroundColor:Red "Error in unit testing"
    exit -1
}

# Prepare for integration test
cd $src_dir/build/$BUILD_TYPE
(Get-Content config.example.json) -replace '"relaunch_on_error.*$', '"relaunch_on_error": true,' | Out-File config.example.json -Encoding utf8
(Get-Content config.example.json) -replace '"threads_num.*$', '"threads_num": 0,' | Out-File config.example.json -Encoding utf8
(Get-Content config.example.json) -replace '"use_brotli.*$', '"use_brotli": true,' | Out-File config.example.json -Encoding utf8

if (!(Test-Path -Path "integration_test_client.exe")) {
	Write-Host -ForegroundColor:Red "Build failed"
    exit -1
}
if (!(Test-Path -Path "integration_test_server.exe")) {
	Write-Host -ForegroundColor:Red "Build failed"
    exit -1
}

# Do integration test
Get-process -Name "integration_test_server" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
#Start-Process -FilePath "./integration_test_server.exe"

sleep 4

Write-Host -ForegroundColor:Cyan "Running the integration test"
#./integration_test_client.exe -s

if ( !$? ) {
    Write-Host -ForegroundColor:Red "Integration test failed"
    exit -1
} 
Get-process -Name "integration_test_server" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# Db and redies Test
cd $src_dir/build/$BUILD_TYPE
if ((Test-Path -Path "db_test")) {
    Write-Host -ForegroundColor:Cyan "Test database"
    ./db_test -s
    if ( !$? ) { 
        Write-Host -ForegroundColor:Red "Error in db testing"
        exit -1
    }
}
if ((Test-Path -Path "redis_test")) {
    Write-Host -ForegroundColor:Cyan "Test redis"
    ./redis_test -s
    if ( !$? ) {
        Write-Host -ForegroundColor:Red "Error in redis testing"
        exit -1
    }
}

# Test drogon_ctl, run at build/$BUILD_TYPE
cd $src_dir/build/$BUILD_TYPE
Write-Host -ForegroundColor:Cyan "Testing drogon_ctl"
$drogon_ctl_exec="$src_dir/build/$BUILD_TYPE/drogon_ctl.exe"

Remove-item $src_dir/build/$BUILD_TYPE/drogon_test -recurse -ErrorAction SilentlyContinue

& $drogon_ctl_exec create project drogon_test

Get-ChildItem .
Get-ChildItem ./drogon_test

cd drogon_test/controllers

& $drogon_ctl_exec create controller Test::SimpleCtrl
& $drogon_ctl_exec create controller -h Test::HttpCtrl
& $drogon_ctl_exec create controller -w Test::WebsockCtrl
& $drogon_ctl_exec create controller SimpleCtrl
& $drogon_ctl_exec create controller -h HttpCtrl
& $drogon_ctl_exec create controller -w WebsockCtrl

if (!(Test-Path -Path "Test_SimpleCtrl.h") -or !(Test-Path -Path "Test_SimpleCtrl.cc") -or !(Test-Path -Path "Test_HttpCtrl.h") -or !(Test-Path -Path "Test_HttpCtrl.cc") ) {
	Write-Host -ForegroundColor:Red "Failed to create controllers"
    exit -1
}
if (!(Test-Path -Path "Test_WebsockCtrl.h") -or !(Test-Path -Path "Test_WebsockCtrl.cc") -or !(Test-Path -Path "SimpleCtrl.h") -or !(Test-Path -Path "SimpleCtrl.cc") ) {
	Write-Host -ForegroundColor:Red "Failed to create controllers"
    exit -1
}
if (!(Test-Path -Path "HttpCtrl.h") -or !(Test-Path -Path "HttpCtrl.cc") -or !(Test-Path -Path "WebsockCtrl.h") -or !(Test-Path -Path "WebsockCtrl.cc") ) {
	Write-Host -ForegroundColor:Red "Failed to create controllers"
    exit -1
}

cd ../filters
& $drogon_ctl_exec create filter Test::TestFilter
if (!(Test-Path -Path "Test_TestFilter.h") -or !(Test-Path -Path "Test_TestFilter.cc") ) {
    Write-Host -ForegroundColor:Red "Failed to create filters"
    exit -1
}

cd ../plugins
& $drogon_ctl_exec create plugin Test::TestPlugin
if (!(Test-Path -Path "Test_TestPlugin.h") -or !(Test-Path -Path "Test_TestPlugin.cc") ) {
    Write-Host -ForegroundColor:Red "Failed to create plugins"
    exit -1
}

cd ../views
"Hello World" | Out-File hello.csp -Encoding utf8

cd ../build
if ("$COMPILER" -eq "Visual Studio") {
   conan install $src_dir -s compiler=$COMPILER -s compiler.version=$COMPILER_VERSION -s build_type=$BUILD_TYPE -g cmake_paths
}
# passing path
$env:path+=";$src_dir/build/install/bin"
$env:path+=";$src_dir/build/install/lib/cmake/Drogon"
$env:path+=";$src_dir/build/install/lib/cmake/Trantor"

if ("$COMPILER" -eq "Visual Studio") {
   cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_TOOLCHAIN_FILE="conan_paths.cmake" -DCMAKE_INSTALL_PREFIX="$src_dir/build/install" "$cmake_gen"
} else {
   cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX="$src_dir/build/install" "$cmake_gen"
}

if ( !$? ) {
    Write-Host -ForegroundColor:Red "Failed to run CMake for example project"
    exit -1
}

cmake --build .
if ( !$? ) {
    Write-Host -ForegroundColor:Red "Error in drogon_test build"
    exit -1
}

if (!(Test-Path -Path "drogon_test.exe")) {
	Write-Host -ForegroundColor:Red "Failed to build drogon_test"
    exit -1
}
# go back to src_path
cd $src_dir

# All test pass
Write-Host -ForegroundColor:Green "Everything is ok!"
exit 0
