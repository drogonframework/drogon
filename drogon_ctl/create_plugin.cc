/**
 *
 *  create_plugin.cc
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

#include "create_plugin.h"
#include <drogon/DrTemplateBase.h>
#include <drogon/utils/Utilities.h>

#include <string>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fstream>
#include <regex>

#include <sys/stat.h>
#include <sys/types.h>

using namespace drogon_ctl;

static void createPluginHeaderFile(std::ofstream &file,
                                   const std::string &className,
                                   const std::string &fileName)
{
    auto templ = drogon::DrTemplateBase::newTemplate("plugin_h");
    HttpViewData data;
    if (className.find("::") != std::string::npos)
    {
        auto namespaceVector = utils::splitString(className, "::");
        data.insert("className", namespaceVector.back());
        namespaceVector.pop_back();
        data.insert("namespaceVector", namespaceVector);
    }
    else
    {
        data.insert("className", className);
    }
    data.insert("filename", fileName);
    file << templ->genText(data);
}

static void createPluginSourceFile(std::ofstream &file,
                                   const std::string &className,
                                   const std::string &fileName)
{
    auto templ = drogon::DrTemplateBase::newTemplate("plugin_cc");
    HttpViewData data;
    if (className.find("::") != std::string::npos)
    {
        auto pos = className.rfind("::");
        data.insert("namespaceString", className.substr(0, pos));
        data.insert("className", className.substr(pos + 2));
    }
    else
    {
        data.insert("className", className);
    }
    data.insert("filename", fileName);
    file << templ->genText(data);
}
void create_plugin::handleCommand(std::vector<std::string> &parameters)
{
    if (parameters.size() < 1)
    {
        std::cout << "Invalid parameters!" << std::endl;
    }
    for (auto className : parameters)
    {
        std::regex regex("::");
        std::string fileName =
            std::regex_replace(className, regex, std::string("_"));

        std::string headFileName = fileName + ".h";
        std::string sourceFilename = fileName + ".cc";
        {
            std::ifstream iHeadFile(headFileName.c_str(), std::ifstream::in);
            std::ifstream iSourceFile(sourceFilename.c_str(),
                                      std::ifstream::in);

            if (iHeadFile || iSourceFile)
            {
                std::cout << "The file you want to create already exists, "
                             "overwrite it(y/n)?"
                          << std::endl;
                auto in = getchar();
                (void)getchar();  // get the return key
                if (in != 'Y' && in != 'y')
                {
                    std::cout << "Abort!" << std::endl;
                    exit(0);
                }
            }
        }
        std::ofstream oHeadFile(headFileName.c_str(), std::ofstream::out);
        std::ofstream oSourceFile(sourceFilename.c_str(), std::ofstream::out);
        if (!oHeadFile || !oSourceFile)
        {
            perror("");
            exit(1);
        }

        std::cout << "create a plugin:" << className << std::endl;
        createPluginHeaderFile(oHeadFile, className, fileName);
        createPluginSourceFile(oSourceFile, className, fileName);
    }
}
