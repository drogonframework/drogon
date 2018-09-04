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
    cmakeFile<<"cmake_minimum_required(VERSION 3.2)\n"
               "Project("<<projectName<<")\n"
               "link_libraries(drogon trantor uuid pthread jsoncpp ssl crypto dl z)\n"
               "#如果您的gcc版本高于等于7.1.0，请设为'17'\n"
               "set(CMAKE_CXX_STANDARD 11)\n"
               "aux_source_directory(./ SRC_DIR)\n"
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
               "add_executable("<<projectName<<" ${SRC_DIR} ${VIEWSRC})";
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
    std::cout<<"create a project named "<<projectName<<std::endl;

    mkdir(projectName.data(),0755);
    //1.create CMakeLists.txt
    chdir(projectName.data());
    std::ofstream cmakeFile("CMakeLists.txt",std::ofstream::out);
    newCmakeFile(cmakeFile,projectName);
    std::ofstream mainFile("main.cc",std::ofstream::out);
    newMainFile(mainFile);
    mkdir("views",0755);
    mkdir("build",0755);
    std::ofstream gitFile(".gitignore",std::ofstream::out);
    newGitIgFile(gitFile);
}