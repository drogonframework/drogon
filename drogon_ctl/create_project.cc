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
               "\n"
               "IF (CMAKE_SYSTEM_NAME MATCHES \"Linux\")\n"
               "    EXEC_PROGRAM (gcc ARGS \"--version | grep '^gcc'|awk '{print $3}' | sed s'/)//g' | sed s'/-.*//g'\" OUTPUT_VARIABLE version)\n"
               "    MESSAGE(STATUS \"This is gcc version:: \" ${version})\n"
               "    if(version LESS 4.9.0)\n"
               "        MESSAGE(STATUS \"gcc is too old\")\n"
               "        stop()\n"
               "    elseif (version LESS 6.1.0)\n"
               "        MESSAGE(STATUS \"c++11\")\n"
               "        set(CMAKE_CXX_STD_FLAGS c++11)\n"
               "    elseif(version LESS 7.1.0)\n"
               "        set(CMAKE_CXX_STD_FLAGS c++14)\n"
               "        MESSAGE(STATUS \"c++14\")\n"
               "    else()\n"
               "        set(CMAKE_CXX_STD_FLAGS c++17)\n"
               "        set(DR_DEFS \"USE_STD_ANY\")\n"
               "        MESSAGE(STATUS \"c++17\")\n"
               "    endif()\n"
               "else()\n"
               "    set(CMAKE_CXX_STD_FLAGS c++11)\n"
               "endif()\n"
               "\n"
               "if(CMAKE_BUILD_TYPE STREQUAL \"\")\n"
               "    set(CMAKE_BUILD_TYPE Release)\n"
               "endif()\n"
               "\n"
               "set(CMAKE_CXX_FLAGS_DEBUG \"${CMAKE_CXX_FLAGS_DEBUG} -Wall -std=${CMAKE_CXX_STD_FLAGS}\")\n"
               "set(CMAKE_CXX_FLAGS_RELEASE \"${CMAKE_CXX_FLAGS_RELEASE} -Wall -std=${CMAKE_CXX_STD_FLAGS}\")\n"
               "\n"
	       "set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules/)\n"
               "#jsoncpp\n"
               "find_package (Jsoncpp REQUIRED)\n"
               "include_directories(${JSONCPP_INCLUDE_DIRS})\n"
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
              "    //Set HTTP listener address and port\n"
              "    drogon::HttpAppFramework::instance().addListener(\"0.0.0.0\",80);\n"
              "    //Run HTTP framework,the method will block in the inner event loop\n"
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
static void newJsonFindFile(std::ofstream &jsonFile)
{
    jsonFile<<"# Find jsoncpp\n"
              "#\n"
              "# Find the jsoncpp includes and library\n"
              "# \n"
              "# if you nee to add a custom library search path, do it via via CMAKE_PREFIX_PATH \n"
              "# \n"
              "# This module defines\n"
              "#  JSONCPP_INCLUDE_DIRS, where to find header, etc.\n"
              "#  JSONCPP_LIBRARIES, the libraries needed to use jsoncpp.\n"
              "#  JSONCPP_FOUND, If false, do not try to use jsoncpp.\n"
              "#  JSONCPP_INCLUDE_PREFIX, include prefix for jsoncpp\n"
              "\n"
              "# only look in default directories\n"
              "find_path(\n"
              "\tJSONCPP_INCLUDE_DIR \n"
              "\tNAMES jsoncpp/json/json.h json/json.h\n"
              "\tDOC \"jsoncpp include dir\"\n"
              ")\n"
              "\n"
              "find_library(\n"
              "\tJSONCPP_LIBRARY\n"
              "\tNAMES jsoncpp\n"
              "\tDOC \"jsoncpp library\"\n"
              ")\n"
              "\n"
              "set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})\n"
              "set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})\n"
              "\n"
              "# debug library on windows\n"
              "# same naming convention as in qt (appending debug library with d)\n"
              "# boost is using the same \"hack\" as us with \"optimized\" and \"debug\"\n"
              "if (\"${CMAKE_CXX_COMPILER_ID}\" STREQUAL \"MSVC\")\n"
              "\tfind_library(\n"
              "\t\tJSONCPP_LIBRARY_DEBUG\n"
              "\t\tNAMES jsoncppd\n"
              "\t\tDOC \"jsoncpp debug library\"\n"
              "\t)\n"
              "\t\n"
              "\tset(JSONCPP_LIBRARIES optimized ${JSONCPP_LIBRARIES} debug ${JSONCPP_LIBRARY_DEBUG})\n"
              "\n"
              "endif()\n"
              "\n"
              "# find JSONCPP_INCLUDE_PREFIX\n"
              "find_path(\n"
              "\tJSONCPP_INCLUDE_PREFIX\n"
              "\tNAMES json.h\n"
              "\tPATH_SUFFIXES jsoncpp/json json\n"
              ")\n"
              "\n"
              "if (${JSONCPP_INCLUDE_PREFIX} MATCHES \"jsoncpp\")\n"
              "\tset(JSONCPP_INCLUDE_PREFIX \"jsoncpp\")\n"
              "\tset(JSONCPP_INCLUDE_DIRS \"${JSONCPP_INCLUDE_DIRS}/jsoncpp\")\n"
              "else()\n"
              "\tset(JSONCPP_INCLUDE_PREFIX \"\")\n"
              "endif()\n"
              "\n"
              "\n"
              "# handle the QUIETLY and REQUIRED arguments and set JSONCPP_FOUND to TRUE\n"
              "# if all listed variables are TRUE, hide their existence from configuration view\n"
              "include(FindPackageHandleStandardArgs)\n"
              "find_package_handle_standard_args(jsoncpp DEFAULT_MSG\n"
              "\tJSONCPP_INCLUDE_DIR JSONCPP_LIBRARY)\n"
              "mark_as_advanced (JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY)\n";
}

static void newConfigFile(std::ofstream &configFile)
{
    configFile<<"/* This is a JSON format configuration file\n"
                "*/\n"
                "{\n"
                "  //ssl:the global ssl files setting\n"
                "  /*\"ssl\": {\n"
                "    \"cert\": \"../../trantor/trantor/tests/server.pem\",\n"
                "    \"key\": \"../../trantor/trantor/tests/server.pem\"\n"
                "  },\n"
                "  \"listeners\": [\n"
                "    {\n"
                "      //address:ip address,0.0.0.0 by default\n"
                "      \"address\": \"0.0.0.0\",\n"
                "      //port:port number\n"
                "      \"port\": 80,\n"
                "      //https:if use https for security,false by default\n"
                "      \"https\": false\n"
                "    },\n"
                "    {\n"
                "      \"address\": \"0.0.0.0\",\n"
                "      \"port\": 443,\n"
                "      \"https\": true,\n"
                "      //cert,key:cert file path and key file path,empty by default,\n"
                "      //if empty,use global setting\n"
                "      \"cert\": \"\",\n"
                "      \"key\": \"\"\n"
                "    }\n"
                "  ],*/\n"
                "  \"app\": {\n"
                "    //threads_num:num of threads,1 by default\n"
                "    \"threads_num\": 16,\n"
                "    //enable_session:false by default\n"
                "    \"enable_session\": false,\n"
                "    \"session_timeout\": 0,\n"
                "    //document_root:Root path of HTTP document,defaut path is ./\n"
                "    \"document_root\": \"./\",\n"
                "    /* file_types:\n"
                "     * HTTP download file types,The file types supported by drogon\n"
                "     * by default are \"html\", \"js\", \"css\", \"xml\", \"xsl\", \"txt\", \"svg\",\n"
                "     * \"ttf\", \"otf\", \"woff2\", \"woff\" , \"eot\", \"png\", \"jpg\", \"jpeg\",\n"
                "     * \"gif\", \"bmp\", \"ico\", \"icns\", etc. */\n"
                "    \"file_types\": [\"gif\",\"png\",\"jpg\",\"js\",\"css\",\"html\",\"ico\",\"swf\",\"xap\",\"apk\",\"cur\",\"xml\"],\n"
                "    //max_connections:max connections number,100000 by default\n"
                "    \"max_connections\": 100000,\n"
                "    //Load_dynamic_views: false by default, when set to true, drogon will\n"
                "    //compile and load dynamically \"CSP View Files\" in directories defined\n"
                "    //by \"dynamic_views_path\"\n"
                "    \"load_dynamic_views\": true,\n"
                "    //dynamic_views_path: if the path isn't prefixed with / or ./,\n"
                "    //it will be relative path of document_root path\n"
                "    \"dynamic_views_path\": [\"./views\"],\n"
                "    //log:set log output,drogon output logs to stdout by default\n"
                "    \"log\": {\n"
                "      //log_path:log file path,empty by default,in which case,log will output to the stdout\n"
                "      \"log_path\": \"./\",\n"
                "      //logfile_base_name:log file base name,empty by default which means 'drogon' will name logfile as\n"
                "      //drogon.log ...\n"
                "      \"logfile_base_name\": \"\",\n"
                "      //log_size_limit:100000000 bytes by default,\n"
                "      //When the log file size reaches \"log_size_limit\", the log file is switched.\n"
                "      \"log_size_limit\": 100000000,\n"
                "      //log_level:\"DEBUG\" by default,options:\"TRACE\",\"DEBUG\",\"INFO\",\"WARN\"\n"
                "      //The TRACE level is only valid when built in DEBUG mode.\n"
                "      \"log_level\": \"DEBUG\"\n"
                "    },\n"
                "    //run_as_daemon:false by default\n"
                "    \"run_as_daemon\": false,\n"
                "    //relaunch_on_error:false by default,if true,the program will be restart by parent after exit;\n"
                "    \"relaunch_on_error\": false,\n"
                "    //use_sendfile:true by default,if ture,the program will\n"
                "    //use sendfile() system-call to send static file to client;\n"
                "    \"use_sendfile\": true,\n"
                "    //use_gzip:true by default,use gzip to compress the response body's content;\n"
                "    \"use_gzip\": true\n"
                "  }\n"
                "}\n";
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
    auto r=chdir(projectName.data());
    (void)(r);
    std::ofstream cmakeFile("CMakeLists.txt",std::ofstream::out);
    newCmakeFile(cmakeFile,projectName);
    std::ofstream mainFile("main.cc",std::ofstream::out);
    newMainFile(mainFile);
    mkdir("views",0755);
    mkdir("controllers",0755);
    mkdir("filters",0755);
    mkdir("build",0755);
    mkdir("cmake_modules",0755);
    std::ofstream jsonFile("cmake_modules/FindJsoncpp.cmake",std::ofstream::out);
    newJsonFindFile(jsonFile);
    std::ofstream gitFile(".gitignore",std::ofstream::out);
    newGitIgFile(gitFile);
    std::ofstream configFile("config.json",std::ofstream::out);
    newConfigFile(configFile);
}
