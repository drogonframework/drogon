/**
 *
 *  create_swagger.cc
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

#include "../lib/src/filesystem.h"
#include "create_swagger.h"
#include "HandlerParser.h"
#include <drogon/DrTemplateBase.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#else
#include <io.h>
#include <direct.h>
#endif
#include <fstream>
#include <regex>

using namespace drogon_ctl;

static void forEachControllerHeaderIn(
    std::string strPath,
    const std::function<void(const std::string &)> &cb)
{
    while (1)
    {
        char cEnd = *strPath.rbegin();
        if (cEnd == '\\' || cEnd == '/')
        {
            strPath = strPath.substr(0, strPath.length() - 1);
        }
        else
        {
            break;
        }
    }

    if (strPath.empty() || strPath == (".") || strPath == (".."))
        return;

    drogon::error_code ec;
    drogon::filesystem::path fsPath(strPath);
    if (!drogon::filesystem::exists(strPath, ec))
    {
        return;
    }
    for (auto &itr : drogon::filesystem::directory_iterator(fsPath))
    {
        if (drogon::filesystem::is_directory(itr.status()))
        {
            forEachControllerHeaderIn(itr.path().string(), cb);
        }
        else
        {
            auto fileName = itr.path().string();
            if (fileName.find(".h") == fileName.length() - 2 ||
                fileName.find(".hpp") == fileName.length() - 4)
            {
                cb(fileName);
            }
        }
    }
    return;
}
static void parseControllerHeader(const std::string &headerFile,
                                  Json::Value &docs)
{
    std::ifstream infile(utils::toNativePath(headerFile),
                         std::ifstream::binary);
    if (!infile)
    {
        std::cout << "can't open the header file:" << headerFile << "\n";
        return;
    }
    std::streambuf *pbuf = infile.rdbuf();
    std::streamsize filesize = pbuf->pubseekoff(0, std::ifstream::end);
    pbuf->pubseekoff(0, std::ifstream::beg);  // rewind
    std::string fileContent;
    fileContent.resize(filesize);
    pbuf->sgetn(&fileContent[0], filesize);
    std::cout << fileContent;

    internal::StructNode r(fileContent, "root", internal::StructNode::kRoot);
    r.print();
}
static std::string makeSwaggerDocument(const Json::Value &config)
{
    Json::Value ret;
    ret["swagger"] = "2.0";
    ret["info"] = config.get("info", {});
    forEachControllerHeaderIn("controllers", [&ret](const std::string &header) {
        std::cout << "Parsing " << header << " ...\n";
        parseControllerHeader(header, ret);
    });
    return ret.toStyledString();
}

static void createSwaggerHeader(const std::string &path,
                                const Json::Value &config)
{
    drogon::HttpViewData data;
    data["docs_url"] = config.get("docs_url", "/swagger").asString();
    std::ofstream ctlHeader(path + "/SwaggerCtrl.h", std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("swagger_h.csp");
    ctlHeader << templ->genText(data);
}

static void createSwaggerSource(const std::string &path,
                                const Json::Value &config)
{
    drogon::HttpViewData data;
    data["docs_string"] = makeSwaggerDocument(config);

    std::ofstream ctlSource(path + "/SwaggerCtrl.cc", std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("swagger_cc.csp");
    ctlSource << templ->genText(data);
}

static void createSwagger(const std::string &path)
{
#ifndef _WIN32
    DIR *dp;
    if ((dp = opendir(path.c_str())) == NULL)
    {
        std::cerr << "No such file or directory : " << path << std::endl;
        return;
    }
    closedir(dp);
#endif
    auto configFile = path + "/swagger.json";
#ifdef _WIN32
    if (_access(configFile.c_str(), 0) != 0)
#else
    if (access(configFile.c_str(), 0) != 0)
#endif
    {
        std::cerr << "Config file " << configFile << " not found!" << std::endl;
        exit(1);
    }
#ifdef _WIN32
    if (_access(configFile.c_str(), 04) != 0)
#else
    if (access(configFile.c_str(), R_OK) != 0)
#endif
    {
        std::cerr << "No permission to read config file " << configFile
                  << std::endl;
        exit(1);
    }

    std::ifstream infile(configFile.c_str(), std::ifstream::in);
    if (infile)
    {
        Json::Value configJsonRoot;
        try
        {
            infile >> configJsonRoot;
            createSwaggerHeader(path, configJsonRoot);
            createSwaggerSource(path, configJsonRoot);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Configuration file format error! in " << configFile
                      << ":" << std::endl;
            std::cerr << exception.what() << std::endl;
            exit(1);
        }
    }
}
void create_swagger::handleCommand(std::vector<std::string> &parameters)
{
    if (parameters.size() < 1)
    {
        std::cout << "please input a path to create the swagger controller"
                  << std::endl;
        exit(1);
    }
    auto path = parameters[0];
    createSwagger(path);
}