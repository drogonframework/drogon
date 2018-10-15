#include "create_project.h"
#include <drogon/HttpResponse.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

using namespace drogon_ctl;

void create_project::handleCommand(std::vector<std::string> &parameters)
{
    if (parameters.size() < 1)
    {
        std::cout << "please input project name" << std::endl;
        exit(1);
    }
    auto pName = parameters[0];
    createProject(pName);
}
static void newCmakeFile(std::ofstream &cmakeFile, const std::string &projectName)
{
    HttpViewData data;
    data.insert("ProjectName",projectName);
    auto resp=HttpResponse::newHttpViewResponse("cmake.csp",data);
    cmakeFile << resp->getBody();
}
static void newMainFile(std::ofstream &mainFile)
{
    auto resp=HttpResponse::newHttpViewResponse("demoMain");
    mainFile << resp->getBody();
}
static void newGitIgFile(std::ofstream &gitFile)
{
    auto resp=HttpResponse::newHttpViewResponse("gitignore.csp");
    gitFile << resp->getBody();
}
static void newJsonFindFile(std::ofstream &jsonFile)
{
    auto resp=HttpResponse::newHttpViewResponse("FindJsoncpp.csp");
    jsonFile << resp->getBody();
}

static void newConfigFile(std::ofstream &configFile)
{
    auto resp=HttpResponse::newHttpViewResponse("config",drogon::HttpViewData());
    configFile << resp->getBody();
}
void create_project::createProject(const std::string &projectName)
{

    if (access(projectName.data(), 0) == 0)
    {
        std::cerr << "The directory already exists, please use another project name!" << std::endl;
        exit(1);
    }
    std::cout << "create a project named " << projectName << std::endl;
    mkdir(projectName.data(), 0755);
    //1.create CMakeLists.txt
    auto r = chdir(projectName.data());
    (void)(r);
    std::ofstream cmakeFile("CMakeLists.txt", std::ofstream::out);
    newCmakeFile(cmakeFile, projectName);
    std::ofstream mainFile("main.cc", std::ofstream::out);
    newMainFile(mainFile);
    mkdir("views", 0755);
    mkdir("controllers", 0755);
    mkdir("filters", 0755);
    mkdir("build", 0755);
    mkdir("cmake_modules", 0755);
    std::ofstream jsonFile("cmake_modules/FindJsoncpp.cmake", std::ofstream::out);
    newJsonFindFile(jsonFile);
    std::ofstream gitFile(".gitignore", std::ofstream::out);
    newGitIgFile(gitFile);
    std::ofstream configFile("config.json", std::ofstream::out);
    newConfigFile(configFile);
}
