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
#include <iostream>
#include <fstream>
#include <regex>

using namespace drogon_ctl;

void create_controller::handleCommand(std::vector<std::string> &parameters)
{
    //std::cout<<"create!"<<std::endl;
    ControllerType type = Simple;
    for (auto iter = parameters.begin(); iter != parameters.end(); iter++)
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
            else
            {
                std::cout << ARGS_ERROR_STR << std::endl;
                return;
            }
        }
    }
    createController(parameters, type);
}

void create_controller::newSimpleControllerHeaderFile(std::ofstream &file, const std::string &className)
{
    file << "#pragma once\n";
    file << "#include <drogon/HttpSimpleController.h>\n";
    file << "using namespace drogon;\n";
    std::string indent = "";
    std::string class_name = className;
    std::string namepace_path = "/";
    auto pos = class_name.find("::");
    while (pos != std::string::npos)
    {
        auto namespaceName = class_name.substr(0, pos);
        class_name = class_name.substr(pos + 2);
        file << indent << "namespace " << namespaceName << "\n";
        namepace_path.append(namespaceName).append("/");
        file << indent << "{\n";
        indent.append("    ");
        pos = class_name.find("::");
    }
    file << indent << "class " << class_name << ":public drogon::HttpSimpleController<" << class_name << ">\n";
    file << indent << "{\n";
    file << indent << "public:\n";
    //TestController(){}
    file << indent << "    virtual void asyncHandleHttpRequest(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)> & callback)override;\n";

    file << indent << "    PATH_LIST_BEGIN\n";
    file << indent << "    //list path definitions here;\n";
    file << indent << "    //PATH_ADD(\"/path\",\"filter1\",\"filter2\",HttpMethod1,HttpMethod2...);\n";
    file << indent << "    PATH_LIST_END\n";
    file << indent << "};\n";
    if (indent == "")
        return;
    do
    {
        indent.resize(indent.length() - 4);
        file << indent << "}\n";
    } while (indent != "");
}
void create_controller::newSimpleControllerSourceFile(std::ofstream &file, const std::string &className, const std::string &filename)
{
    file << "#include \"" << filename << ".h\"\n";
    auto pos = className.rfind("::");
    auto class_name = className;
    if (pos != std::string::npos)
    {
        auto namespacename = className.substr(0, pos);
        file << "using namespace " << namespacename << ";\n";
        class_name = className.substr(pos + 2);
    }
    file << "void " << class_name << "::asyncHandleHttpRequest(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)> & callback)\n";
    file << "{\n";
    file << "    //write your application logic here\n";
    file << "}";
}

void create_controller::newWebsockControllerHeaderFile(std::ofstream &file, const std::string &className)
{
    file << "#pragma once\n";
    file << "#include <drogon/WebSocketController.h>\n";
    file << "using namespace drogon;\n";
    std::string indent = "";
    std::string class_name = className;
    std::string namepace_path = "/";
    auto pos = class_name.find("::");
    while (pos != std::string::npos)
    {
        auto namespaceName = class_name.substr(0, pos);
        class_name = class_name.substr(pos + 2);
        file << indent << "namespace " << namespaceName << "\n";
        namepace_path.append(namespaceName).append("/");
        file << indent << "{\n";
        indent.append("    ");
        pos = class_name.find("::");
    }
    file << indent << "class " << class_name << ":public drogon::WebSocketController<" << class_name << ">\n";
    file << indent << "{\n";
    file << indent << "public:\n";
    //TestController(){}
    //    virtual void handleNewMessage(const WebSocketConnectionPtr&,
    //                                  trantor::MsgBuffer*)=0;
    //    //on new connection or after disconnect
    //    virtual void handleConnection(const WebSocketConnectionPtr&)=0;
    file << indent << "    virtual void handleNewMessage(const WebSocketConnectionPtr&,\n";
    file << indent << "                                  std::string &&)override;\n";
    file << indent << "    virtual void handleNewConnection(const HttpRequestPtr &,\n";
    file << indent << "                                     const WebSocketConnectionPtr&)override;\n";
    file << indent << "    virtual void handleConnectionClosed(const WebSocketConnectionPtr&)override;\n";
    file << indent << "    WS_PATH_LIST_BEGIN\n";
    file << indent << "    //list path definitions here;\n";
    file << indent << "    //WS_PATH_ADD(\"/path\",\"filter1\",\"filter2\",...);\n";
    file << indent << "    WS_PATH_LIST_END\n";
    file << indent << "};\n";
    if (indent == "")
        return;
    do
    {
        indent.resize(indent.length() - 4);
        file << indent << "}\n";
    } while (indent != "");
}
void create_controller::newWebsockControllerSourceFile(std::ofstream &file, const std::string &className, const std::string &filename)
{
    file << "#include \"" << filename << ".h\"\n";
    auto pos = className.rfind("::");
    auto class_name = className;
    if (pos != std::string::npos)
    {
        auto namespacename = className.substr(0, pos);
        file << "using namespace " << namespacename << ";\n";
        class_name = className.substr(pos + 2);
    }
    file << "void " << class_name << "::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,std::string &&message)\n";
    file << "{\n";
    file << "    //write your application logic here\n";
    file << "}\n";
    file << "void " << class_name << "::handleNewConnection(const HttpRequestPtr &req,const WebSocketConnectionPtr& wsConnPtr)\n";
    file << "{\n";
    file << "    //write your application logic here\n";
    file << "}\n";
    file << "void " << class_name << "::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)\n";
    file << "{\n";
    file << "    //write your application logic here\n";
    file << "}\n";
}

void create_controller::newHttpControllerHeaderFile(std::ofstream &file, const std::string &className)
{
    file << "#pragma once\n";
    file << "#include <drogon/HttpController.h>\n";
    file << "using namespace drogon;\n";
    std::string indent = "";
    std::string class_name = className;
    std::string namepace_path = "/";
    auto pos = class_name.find("::");
    while (pos != std::string::npos)
    {
        auto namespaceName = class_name.substr(0, pos);
        class_name = class_name.substr(pos + 2);
        file << indent << "namespace " << namespaceName << "\n";
        namepace_path.append(namespaceName).append("/");
        file << indent << "{\n";
        indent.append("    ");
        pos = class_name.find("::");
    }
    file << indent << "class " << class_name << ":public drogon::HttpController<" << class_name << ">\n";
    file << indent << "{\n";
    file << indent << "public:\n";
    indent.append("    ");
    file << indent << "METHOD_LIST_BEGIN\n";
    file << indent << "//use METHOD_ADD to add your custom processing function here;\n";
    file << indent << "//METHOD_ADD(" << class_name << "::get,\"/get/{2}/{1}\",Get);"
                                                       "//path will be "
         << namepace_path << class_name << "/get/{arg2}/{arg1}\n";
    file << indent << "//METHOD_ADD(" << class_name << "::your_method_name,\"/{1}/{2}/list\",\"drogon::GetFilter\");"
                                                       "//path will be "
         << namepace_path << class_name << "/{arg1}/{arg2}/list\n";
    file << indent << "\n";
    file << indent << "METHOD_LIST_END\n";
    file << indent << "//your declaration of processing function maybe like this:\n";
    file << indent << "//void get(const HttpRequestPtr& req,"
                      "const std::function<void (const HttpResponsePtr &)>&callback,int p1,std::string p2);\n";
    file << indent << "//void your_method_name(const HttpRequestPtr& req,"
                      "const std::function<void (const HttpResponsePtr &)>&callback,double p1,int p2) const;\n";
    indent.resize(indent.length() - 4);
    file << indent << "};\n";
    if (indent == "")
        return;
    do
    {
        indent.resize(indent.length() - 4);
        file << indent << "}\n";
    } while (indent != "");
}
void create_controller::newHttpControllerSourceFile(std::ofstream &file, const std::string &className, const std::string &filename)
{
    file << "#include \"" << filename << ".h\"\n";
    auto pos = className.rfind("::");
    auto class_name = className;
    if (pos != std::string::npos)
    {
        auto namespacename = className.substr(0, pos);
        file << "using namespace " << namespacename << ";\n";
        class_name = className.substr(pos + 2);
    }

    file << "//add definition of your processing function here\n";
}

void create_controller::createController(std::vector<std::string> &httpClasses, ControllerType type)
{
    for (auto iter = httpClasses.begin(); iter != httpClasses.end(); iter++)
    {
        if ((*iter)[0] == '-')
        {
            std::cout << ARGS_ERROR_STR << std::endl;
            return;
        }
    }
    for (auto className : httpClasses)
    {
        createController(className, type);
    }
}

void create_controller::createController(const std::string &className, ControllerType type)
{
    std::regex regex("::");
    std::string ctlName = std::regex_replace(className, regex, std::string("_"));

    std::string headFileName = ctlName + ".h";
    std::string sourceFilename = ctlName + ".cc";
    {
        std::ifstream iHeadFile(headFileName.c_str(), std::ifstream::in);
        std::ifstream iSourceFile(sourceFilename.c_str(), std::ifstream::in);

        if (iHeadFile || iSourceFile)
        {
            std::cout << "The file you want to create already exists, overwrite it(y/n)?" << std::endl;
            auto in = getchar();
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
        exit(-1);
    }
    if (type == Http)
    {
        std::cout << "create a http controller:" << className << std::endl;
        newHttpControllerHeaderFile(oHeadFile, className);
        newHttpControllerSourceFile(oSourceFile, className, ctlName);
    }
    else if (type == Simple)
    {
        std::cout << "create a http simple controller:" << className << std::endl;
        newSimpleControllerHeaderFile(oHeadFile, className);
        newSimpleControllerSourceFile(oSourceFile, className, ctlName);
    }
    else if (type == WebSocket)
    {
        std::cout << "create a websocket controller:" << className << std::endl;
        newWebsockControllerHeaderFile(oHeadFile, className);
        newWebsockControllerSourceFile(oSourceFile, className, ctlName);
    }
}
