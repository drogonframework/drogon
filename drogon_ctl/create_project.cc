/**
 *
 *  create_project.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "create_project.h"
#include <drogon/DrTemplateBase.h>
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#include <direct.h>
#endif
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
static void newCmakeFile(std::ofstream &cmakeFile,
                         const std::string &projectName)
{
    HttpViewData data;
    data.insert("ProjectName", projectName);
    auto templ = DrTemplateBase::newTemplate("cmake.csp");
    cmakeFile << templ->genText(data);
}
static void newMainFile(std::ofstream &mainFile)
{
    auto templ = DrTemplateBase::newTemplate("demoMain");
    mainFile << templ->genText();
}
static void newGitIgFile(std::ofstream &gitFile)
{
    auto templ = DrTemplateBase::newTemplate("gitignore.csp");
    gitFile << templ->genText();
}

static void newConfigFile(std::ofstream &configFile)
{
    auto templ = DrTemplateBase::newTemplate("config");
    configFile << templ->genText();
}
static void newModelConfigFile(std::ofstream &configFile)
{
    auto templ = DrTemplateBase::newTemplate("model_json");
    configFile << templ->genText();
}
static void newSwaggerConfigFile(std::ofstream &configFile)
{
    auto templ = DrTemplateBase::newTemplate("swagger_json");
    configFile << templ->genText();
}
static void newTestMainFile(std::ofstream &mainFile)
{
    auto templ = DrTemplateBase::newTemplate("test_main");
    mainFile << templ->genText();
}
static void newTestCmakeFile(std::ofstream &testCmakeFile,
                             const std::string &projectName)
{
    HttpViewData data;
    data.insert("ProjectName", projectName);
    auto templ = DrTemplateBase::newTemplate("test_cmake");
    testCmakeFile << templ->genText(data);
}
void create_project::createProject(const std::string &projectName)
{
#ifdef _WIN32
    if (_access(projectName.data(), 0) == 0)
#else
    if (access(projectName.data(), 0) == 0)
#endif
    {
        std::cerr
            << "The directory already exists, please use another project name!"
            << std::endl;
        exit(1);
    }
    std::cout << "create a project named " << projectName << std::endl;

    drogon::utils::createPath(projectName);
// 1.create CMakeLists.txt
#ifdef _WIN32
    auto r = _chdir(projectName.data());
#else
    auto r = chdir(projectName.data());
#endif
    (void)(r);
    std::ofstream cmakeFile("CMakeLists.txt", std::ofstream::out);
    newCmakeFile(cmakeFile, projectName);
    std::ofstream mainFile("main.cc", std::ofstream::out);
    newMainFile(mainFile);
    drogon::utils::createPath("views");
    drogon::utils::createPath("controllers");
    drogon::utils::createPath("filters");
    drogon::utils::createPath("plugins");
    drogon::utils::createPath("build");
    drogon::utils::createPath("models");
    drogon::utils::createPath("test");
    drogon::utils::createPath("swagger");

    std::ofstream gitFile(".gitignore", std::ofstream::out);
    newGitIgFile(gitFile);
    std::ofstream configFile("config.json", std::ofstream::out);
    newConfigFile(configFile);
    std::ofstream modelConfigFile("models/model.json", std::ofstream::out);
    newModelConfigFile(modelConfigFile);
    std::ofstream swaggerConfigFile("swagger/swagger.json", std::ofstream::out);
    newSwaggerConfigFile(swaggerConfigFile);
    std::ofstream testMainFile("test/test_main.cc", std::ofstream::out);
    newTestMainFile(testMainFile);
    std::ofstream testCmakeFile("test/CMakeLists.txt", std::ofstream::out);
    newTestCmakeFile(testCmakeFile, projectName);
}
