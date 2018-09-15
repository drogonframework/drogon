#include "create_project.h"
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

using namespace drogon_ctl;

void create_project::handleCommand(std::vector<std::string> &parameters)
{
    if(parameters.size()<1)
    {
        std::cout<<"please input project name"<<std::endl;
        exit(1);
    }
    auto pName=parameters[0];
    createProject(pName);
}
static void newCmakeFile(std::ofstream &cmakeFile,const std::string &projectName)
{
    cmakeFile<< "cmake_minimum_required(VERSION 3.2)\n"
                "Project("
             <<projectName
             <<")\n"
               "link_libraries(drogon trantor uuid pthread jsoncpp dl z)\n"
               "find_package (OpenSSL)\n"
               "if(OpenSSL_FOUND)\n"
               "link_libraries(ssl crypto)\n"
               "endif()\n"
               "IF (CMAKE_SYSTEM_NAME MATCHES \"Linux\")\n"
               "    EXEC_PROGRAM (gcc ARGS \"--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g' | sed s'/-.*//g'\" OUTPUT_VARIABLE version)\n"
               "    MESSAGE(STATUS \"This is gcc version:: \" ${version})\n"
               "    if(version LESS 4.9.0)\n"
               "        MESSAGE(STATUS \"gcc is too old\")\n"
               "        stop()\n"
               "    endif()\n"
               "    include(CheckCXXCompilerFlag)\n"
               "    CHECK_CXX_COMPILER_FLAG(\"-std=c++11\" COMPILER_SUPPORTS_CXX11)\n"
               "    CHECK_CXX_COMPILER_FLAG(\"-std=c++14\" COMPILER_SUPPORTS_CXX14)\n"
               "    CHECK_CXX_COMPILER_FLAG(\"-std=c++17\" COMPILER_SUPPORTS_CXX17)\n"
               "    if(COMPILER_SUPPORTS_CXX17)\n"
               "        message(STATUS \"support c++17\")\n"
               "        set(CMAKE_CXX_STD_FLAGS c++17)\n"
               "        set(DEFS \"USE_STD_ANY\")\n"
               "    elseif(COMPILER_SUPPORTS_CXX14)\n"
               "        message(STATUS \"support c++14\")\n"
               "        set(CMAKE_CXX_STD_FLAGS c++14)\n"
               "    elseif(COMPILER_SUPPORTS_CXX11)\n"
               "        message(STATUS \"support c++11\")\n"
               "        set(CMAKE_CXX_STD_FLAGS c++11)\n"
               "    endif()\n"
               "else()\n"
               "    set(CMAKE_CXX_STD_FLAGS c++11)\n"
               "endif()\n"
               "if(CMAKE_BUILD_TYPE STREQUAL \"\")\n"
               "    set(CMAKE_BUILD_TYPE Release)\n"
               "endif()\n"
               "\n"
               "set(CMAKE_CXX_FLAGS_DEBUG \"${CMAKE_CXX_FLAGS_DEBUG} -Wall -std=${CMAKE_CXX_STD_FLAGS}\")\n"
               "set(CMAKE_CXX_FLAGS_RELEASE \"${CMAKE_CXX_FLAGS_RELEASE} -Wall -std=${CMAKE_CXX_STD_FLAGS}\")\n"
               "\n"
               "AUX_SOURCE_DIRECTORY(./ SRC_DIR)\n"
               "AUX_SOURCE_DIRECTORY(controllers CTL_SRC)\n"
               "AUX_SOURCE_DIRECTORY(filters FILTER_SRC)\n"
               "\n"
               "FILE(GLOB SCP_LIST ${CMAKE_CURRENT_SOURCE_DIR}/views/*.csp)\n"
               "foreach(cspFile ${SCP_LIST})\n"
               "    message(STATUS \"cspFile:\" ${cspFile})\n"
               "    EXEC_PROGRAM(basename ARGS \"-s .csp ${cspFile}\" OUTPUT_VARIABLE classname)\n"
               "    message(STATUS \"view classname:\" ${classname})\n"
               "    add_custom_command(OUTPUT ${classname}.h ${classname}.cc\n"
               "            COMMAND drogon_ctl\n"
               "            ARGS create view ${cspFile}\n"
               "            DEPENDS ${cspFile}\n"
               "            VERBATIM )\n"
               "    set(VIEWSRC ${VIEWSRC} ${classname}.cc)\n"
               "endforeach()\n"
               "\n"
               "add_executable("<<projectName<<" ${SRC_DIR} ${CTL_SRC} ${FILTER_SRC} ${VIEWSRC})\n";
}
static void newMainFile(std::ofstream &mainFile)
{
    mainFile<<"#include <drogon/HttpAppFramework.h>\n"
              "int main() {\n"
              "    //设置http监听的地址和端口\n"
              "    drogon::HttpAppFramework::instance().addListener(\"0.0.0.0\",80);\n"
              "    //运行http框架，程序阻塞在底层的事件循环中\n"
              "    drogon::HttpAppFramework::instance().run();\n"
              "    return 0;\n"
              "}";
}
static void newGitIgFile(std::ofstream &gitFile)
{
    gitFile<<"# Prerequisites\n"
             "*.d\n"
             "\n"
             "# Compiled Object files\n"
             "*.slo\n"
             "*.lo\n"
             "*.o\n"
             "*.obj\n"
             "\n"
             "# Precompiled Headers\n"
             "*.gch\n"
             "*.pch\n"
             "\n"
             "# Compiled Dynamic libraries\n"
             "*.so\n"
             "*.dylib\n"
             "*.dll\n"
             "\n"
             "# Fortran module files\n"
             "*.mod\n"
             "*.smod\n"
             "\n"
             "# Compiled Static libraries\n"
             "*.lai\n"
             "*.la\n"
             "*.a\n"
             "*.lib\n"
             "\n"
             "# Executables\n"
             "*.exe\n"
             "*.out\n"
             "*.app\n"
             "\n"
             "build\n"
             "cmake-build-debug\n"
             ".idea\n";
}
void create_project::createProject(const std::string &projectName)
{

    if(access(projectName.data(),0)==0)
    {
        std::cerr<<"The directory already exists, please use another project name!"<<std::endl;
        exit(1);
    }
    std::cout<<"create a project named "<<projectName<<std::endl;
    mkdir(projectName.data(),0755);
    //1.create CMakeLists.txt
    chdir(projectName.data());
    std::ofstream cmakeFile("CMakeLists.txt",std::ofstream::out);
    newCmakeFile(cmakeFile,projectName);
    std::ofstream mainFile("main.cc",std::ofstream::out);
    newMainFile(mainFile);
    mkdir("views",0755);
    mkdir("controllers",0755);
    mkdir("filters",0755);
    mkdir("build",0755);
    std::ofstream gitFile(".gitignore",std::ofstream::out);
    newGitIgFile(gitFile);
}