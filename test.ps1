# This script is used for windows only,
# if you are using linux/macOS, please use test.sh instead of it.
# conan only provide all x64 dependencies, so all test need baseded on x64.

param(
    [string]$BUILD_TYPE = 'Debug',
    [Int]$COMPILER_VERSION = 16,
    [Int]$VS_VERSION = 2019
)

Write-Host -ForegroundColor:Yellow "BUILD_TYPE:$BUILD_TYPE"
Write-Host -ForegroundColor:Yellow "VS COMPILER_VERSION:$COMPILER_VERSION"
Write-Host -ForegroundColor:Yellow "VS VERSION:$VS_VERSION"
$OS=$env:OS
Write-Host -ForegroundColor:Yellow "OS:$OS"
if ($OS -ne "Windows_NT"){
	Write-Host -ForegroundColor:Yellow "This script is for windows, please use test.sh for other OS"
	exit -1
}

Write-Host -ForegroundColor:Green '
# Install conan first, download dependencies(only support x64), then build project as bellow:
mkdir build
cd build
conan install .. -s compiler="Visual Studio" -s compiler.version=16 -s build_type=Debug -g cmake_paths
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_DROGON_SHARED=ON -DBUILD_CTL=ON -DBUILD_EXAMPLES=ON -DCMAKE_INSTALL_PREFIX=$ABSOLUTE_PATH/install -DCMAKE_TOOLCHAIN_FILE=$ABSOLUTE_PATH/conan_paths.cmake
cmake --build . --parallel --target install

# Using params for this script as bellow: 
test.ps1 Debug 16 2019
test.ps1 -BUILD_TYPE Debug -COMPILER_VERSION 16 -VS_VERSION 2019
 '
$src_dir=$PWD
# set arch to x64
Import-Module ("C:\Program Files (x86)\Microsoft Visual Studio\$VS_VERSION\Enterprise\Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath "C:\Program Files (x86)\Microsoft Visual Studio\$VS_VERSION\Professional" -DevCmdArguments "-arch=x64 -host_arch=x64" -SkipAutomaticLocation

$make_flags=''
$cmake_gen=''
$cmake_test_target='test'
$parallel=$env:NUMBER_OF_PROCESSORS

if ((Test-Path -IsValid "Ninja")) {
	$cmake_gen='-GNinja'
} else{
    $make_flags="$make_flags -j$parallel"
    $cmake_test_target='RUN_TESTS'
}

# Unit testing
cd $src_dir/build
Write-Host -ForegroundColor:Cyan "Unit testing"
cmake --build .  --target $cmake_test_target $make_flags

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

Get-Item .
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
conan install $src_dir -s compiler="Visual Studio" -s compiler.version=$COMPILER_VERSION -s build_type=$BUILD_TYPE -g cmake_paths
$test_conan_paths_cmake="$PWD/conan_paths.cmake"

# passing path
$env:path+=";$src_dir/build/install/bin"
$env:path+=";$src_dir/build/install/lib/cmake/Drogon"
$env:path+=";$src_dir/build/install/lib/cmake/Trantor"

cmake .. "-DCMAKE_TOOLCHAIN_FILE=$test_conan_paths_cmake" "$cmake_gen"
if ( !$? ) {
    Write-Host -ForegroundColor:Red "Failed to run CMake for example project"
    exit -1
}

cmake --build . $make_flags
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
