/**
 *
 *  create_controller.cc
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

#include "create_controller.h"
#include "cmd.h"
#include <drogon/DrTemplateBase.h>
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <fstream>
#include <regex>

using namespace drogon_ctl;

void create_controller::handleCommand(std::vector<std::string> &parameters)
{
    // std::cout<<"create!"<<std::endl;
    ControllerType type = Simple;
    for (auto iter = parameters.begin(); iter != parameters.end(); ++iter)
    {
        if ((*iter)[0] == '-')
        {
            if (*iter == "-s" || (*iter == "--simple"))
            {
                parameters.erase(iter);
                break;
            }
            else if (*iter == "-a" || *iter == "-h" || *iter == "--http")
            {
                type = Http;
                parameters.erase(iter);
                break;
            }
            else if (*iter == "-w" || *iter == "--websocket")
            {
                type = WebSocket;
                parameters.erase(iter);
                break;
            }
            else if (*iter == "-r" || *iter == "--restful")
            {
                type = Restful;
                parameters.erase(iter);
                break;
            }
            else
            {
                std::cout << ARGS_ERROR_STR << std::endl;
                return;
            }
        }
    }
    if (type != Restful)
        createController(parameters, type);
    else
    {
        std::string resource;
        for (auto iter = parameters.begin(); iter != parameters.end(); ++iter)
        {
            if ((*iter).find("--resource=") == 0)
            {
                resource = (*iter).substr(strlen("--resource="));
                parameters.erase(iter);
                break;
            }
            if ((*iter)[0] == '-')
            {
                std::cerr << "Error parameter for '" << (*iter) << "'"
                          << std::endl;
                exit(1);
            }
        }
        if (parameters.size() > 1)
        {
            std::cerr << "Too many parameters" << std::endl;
            exit(1);
        }
        auto className = parameters[0];
        createARestfulController(className, resource);
    }
}

void create_controller::newSimpleControllerHeaderFile(
    std::ofstream &file,
    const std::string &className)
{
    file << "#pragma once\n";
    file << "\n";
    file << "#include <drogon/HttpSimpleController.h>\n";
    file << "\n";
    file << "using namespace drogon;\n";
    file << "\n";
    std::string class_name = className;
    std::string namepace_path = "/";
    auto pos = class_name.find("::");
    size_t namespaceCount = 0;
    while (pos != std::string::npos)
    {
        ++namespaceCount;
        auto namespaceName = class_name.substr(0, pos);
        class_name = class_name.substr(pos + 2);
        file << "namespace " << namespaceName << "\n";
        namepace_path.append(namespaceName).append("/");
        file << "{\n";
        pos = class_name.find("::");
    }
    file << "class " << class_name << " : public drogon::HttpSimpleController<"
         << class_name << ">\n";
    file << "{\n";
    file << "  public:\n";
    file << "    virtual void asyncHandleHttpRequest(const HttpRequestPtr& "
            "req, std::function<void (const HttpResponsePtr &)> &&callback) "
            "override;\n";

    file << "    PATH_LIST_BEGIN\n";
    file << "    // list path definitions here;\n";
    file << "    "
            "// PATH_ADD(\"/"
            "path\", \"filter1\", \"filter2\", HttpMethod1, HttpMethod2...);\n";
    file << "    PATH_LIST_END\n";
    file << "};\n";
    while (namespaceCount > 0)
    {
        --namespaceCount;
        file << "}\n";
    }
}
void create_controller::newSimpleControllerSourceFile(
    std::ofstream &file,
    const std::string &className,
    const std::string &filename)
{
    file << "#include \"" << filename << ".h\"\n";
    file << "\n";
    auto pos = className.rfind("::");
    auto class_name = className;
    if (pos != std::string::npos)
    {
        auto namespacename = className.substr(0, pos);
        file << "using namespace " << namespacename << ";\n";
        file << "\n";
        class_name = className.substr(pos + 2);
    }
    file << "void " << class_name
         << "::asyncHandleHttpRequest(const HttpRequestPtr& req, "
            "std::function<void (const HttpResponsePtr &)> &&callback)\n";
    file << "{\n";
    file << "    // write your application logic here\n";
    file << "}\n";
}

void create_controller::newWebsockControllerHeaderFile(
    std::ofstream &file,
    const std::string &className)
{
    file << "#pragma once\n";
    file << "\n";
    file << "#include <drogon/WebSocketController.h>\n";
    file << "\n";
    file << "using namespace drogon;\n";
    file << "\n";
    std::string class_name = className;
    std::string namepace_path = "/";
    auto pos = class_name.find("::");
    size_t namespaceCount = 0;
    while (pos != std::string::npos)
    {
        ++namespaceCount;
        auto namespaceName = class_name.substr(0, pos);
        class_name = class_name.substr(pos + 2);
        file << "namespace " << namespaceName << "\n";
        namepace_path.append(namespaceName).append("/");
        file << "{\n";
        pos = class_name.find("::");
    }
    file << "class " << class_name << " : public drogon::WebSocketController<"
         << class_name << ">\n";
    file << "{\n";
    file << "  public:\n";
    file
        << "    virtual void handleNewMessage(const WebSocketConnectionPtr&,\n";
    file << "                                  std::string &&,\n";
    file << "                                  const WebSocketMessageType &) "
            "override;\n";
    file << "    virtual void handleNewConnection(const HttpRequestPtr &,\n";
    file << "                                     const "
            "WebSocketConnectionPtr&) override;\n";
    file << "    virtual void handleConnectionClosed(const "
            "WebSocketConnectionPtr&) override;\n";
    file << "    WS_PATH_LIST_BEGIN\n";
    file << "    // list path definitions here;\n";
    file << "    // WS_PATH_ADD(\"/path\", \"filter1\", \"filter2\", ...);\n";
    file << "    WS_PATH_LIST_END\n";
    file << "};\n";
    while (namespaceCount > 0)
    {
        --namespaceCount;
        file << "}\n";
    }
}
void create_controller::newWebsockControllerSourceFile(
    std::ofstream &file,
    const std::string &className,
    const std::string &filename)
{
    file << "#include \"" << filename << ".h\"\n";
    file << "\n";
    auto pos = className.rfind("::");
    auto class_name = className;
    if (pos != std::string::npos)
    {
        auto namespacename = className.substr(0, pos);
        file << "using namespace " << namespacename << ";\n";
        file << "\n";
        class_name = className.substr(pos + 2);
    }
    file << "void " << class_name
         << "::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr, "
            "std::string &&message, const WebSocketMessageType &type)\n";
    file << "{\n";
    file << "    // write your application logic here\n";
    file << "}\n";
    file << "\n";
    file << "void " << class_name
         << "::handleNewConnection(const HttpRequestPtr &req, const "
            "WebSocketConnectionPtr& wsConnPtr)\n";
    file << "{\n";
    file << "    // write your application logic here\n";
    file << "}\n";
    file << "\n";
    file << "void " << class_name
         << "::handleConnectionClosed(const WebSocketConnectionPtr& "
            "wsConnPtr)\n";
    file << "{\n";
    file << "    // write your application logic here\n";
    file << "}\n";
}

void create_controller::newHttpControllerHeaderFile(
    std::ofstream &file,
    const std::string &className)
{
    file << "#pragma once\n";
    file << "\n";
    file << "#include <drogon/HttpController.h>\n";
    file << "\n";
    file << "using namespace drogon;\n";
    file << "\n";
    std::string class_name = className;
    std::string namepace_path = "/";
    auto pos = class_name.find("::");
    size_t namespaceCount = 0;
    while (pos != std::string::npos)
    {
        ++namespaceCount;
        auto namespaceName = class_name.substr(0, pos);
        class_name = class_name.substr(pos + 2);
        file << "namespace " << namespaceName << "\n";
        namepace_path.append(namespaceName).append("/");
        file << "{\n";
        pos = class_name.find("::");
    }
    file << "class " << class_name << " : public drogon::HttpController<"
         << class_name << ">\n";
    file << "{\n";
    file << "  public:\n";
    file << "    METHOD_LIST_BEGIN\n";
    file << "    // use METHOD_ADD to add your custom processing function "
            "here;\n";
    file << "    // METHOD_ADD(" << class_name
         << "::get, \"/{2}/{1}\", Get);"
            " // path is "
         << namepace_path << class_name << "/{arg2}/{arg1}\n";
    file << "    // METHOD_ADD(" << class_name
         << "::your_method_name, \"/{1}/{2}/list\", Get);"
            " // path is "
         << namepace_path << class_name << "/{arg1}/{arg2}/list\n";
    file << "    // ADD_METHOD_TO(" << class_name
         << "::your_method_name, \"/absolute/path/{1}/{2}/list\", Get);"
            " // path is /absolute/path/{arg1}/{arg2}/list\n";
    file << "\n";
    file << "    METHOD_LIST_END\n";
    file << "    // your declaration of processing function maybe like this:\n";
    file << "    // void get(const HttpRequestPtr& req, "
            "std::function<void (const HttpResponsePtr &)> &&callback, int "
            "p1, std::string p2);\n";
    file << "    // void your_method_name(const HttpRequestPtr& req, "
            "std::function<void (const HttpResponsePtr &)> &&callback, double "
            "p1, int p2) const;\n";
    file << "};\n";
    while (namespaceCount > 0)
    {
        --namespaceCount;
        file << "}\n";
    }
}
void create_controller::newHttpControllerSourceFile(
    std::ofstream &file,
    const std::string &className,
    const std::string &filename)
{
    file << "#include \"" << filename << ".h\"\n";
    file << "\n";
    auto pos = className.rfind("::");
    auto class_name = className;
    if (pos != std::string::npos)
    {
        auto namespacename = className.substr(0, pos);
        file << "using namespace " << namespacename << ";\n";
        file << "\n";
        class_name = className.substr(pos + 2);
    }

    file << "// Add definition of your processing function here\n";
}

void create_controller::createController(std::vector<std::string> &httpClasses,
                                         ControllerType type)
{
    for (auto iter = httpClasses.begin(); iter != httpClasses.end(); ++iter)
    {
        if ((*iter)[0] == '-')
        {
            std::cout << ARGS_ERROR_STR << std::endl;
            return;
        }
    }
    for (auto const &className : httpClasses)
    {
        createController(className, type);
    }
}

void create_controller::createController(const std::string &className,
                                         ControllerType type)
{
    std::regex regex("::");
    std::string ctlName =
        std::regex_replace(className, regex, std::string("_"));

    std::string headFileName = ctlName + ".h";
    std::string sourceFilename = ctlName + ".cc";
    {
        std::ifstream iHeadFile(headFileName.c_str(), std::ifstream::in);
        std::ifstream iSourceFile(sourceFilename.c_str(), std::ifstream::in);

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
    if (type == Http)
    {
        std::cout << "Create a http controller: " << className << std::endl;
        newHttpControllerHeaderFile(oHeadFile, className);
        newHttpControllerSourceFile(oSourceFile, className, ctlName);
    }
    else if (type == Simple)
    {
        std::cout << "Create a http simple controller: " << className
                  << std::endl;
        newSimpleControllerHeaderFile(oHeadFile, className);
        newSimpleControllerSourceFile(oSourceFile, className, ctlName);
    }
    else if (type == WebSocket)
    {
        std::cout << "Create a websocket controller: " << className
                  << std::endl;
        newWebsockControllerHeaderFile(oHeadFile, className);
        newWebsockControllerSourceFile(oSourceFile, className, ctlName);
    }
}

void create_controller::createARestfulController(const std::string &className,
                                                 const std::string &resource)
{
    std::regex regex("::");
    std::string ctlName =
        std::regex_replace(className, regex, std::string("_"));

    std::string headFileName = ctlName + ".h";
    std::string sourceFilename = ctlName + ".cc";
    {
        std::ifstream iHeadFile(headFileName.c_str(), std::ifstream::in);
        std::ifstream iSourceFile(sourceFilename.c_str(), std::ifstream::in);

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
    auto v = utils::splitString(className, "::");
    drogon::DrTemplateData data;
    data.insert("className", v[v.size() - 1]);
    v.pop_back();
    data.insert("namespaceVector", v);
    data.insert("resource", resource);
    data.insert("fileName", ctlName);
    if (resource.empty())
    {
        data.insert("ctlCommand",
                    std::string("drogon_ctl create controller -r ") +
                        className);
    }
    else
    {
        data.insert("ctlCommand",
                    std::string("drogon_ctl create controller -r ") +
                        className + " --resource=" + resource);
    }
    try
    {
        auto templ = DrTemplateBase::newTemplate("restful_controller_h.csp");
        oHeadFile << templ->genText(data);
        templ = DrTemplateBase::newTemplate("restful_controller_cc.csp");
        oSourceFile << templ->genText(data);
    }
    catch (const std::exception &err)
    {
        std::cerr << err.what() << std::endl;
        exit(1);
    }
    std::cout << "Create a http restful API controller: " << className
              << std::endl;
    std::cout << "File name: " << ctlName << ".h and " << ctlName << ".cc"
              << std::endl;
}