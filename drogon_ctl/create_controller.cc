//
// Copyright 2018, An Tao.  All rights reserved.
//
// Use of this source code is governed by a MIT license
// that can be found in the License file.

#include "create_controller.h"
#include "cmd.h"
#include <iostream>
#include <fstream>
using namespace drogon_ctl;

void create_controller::handleCommand(std::vector<std::string> &parameters)
{
    //std::cout<<"create!"<<std::endl;
    ControllerType type=Simple;
    for(auto iter=parameters.begin();iter!=parameters.end();iter++)
    {
        if((*iter)[0]=='-')
        {
            if(*iter=="-s"||(*iter=="--simple"))
            {
                parameters.erase(iter);
                break;
            }
            else if(*iter=="-a"||*iter=="--api")
            {
                type=API;
                parameters.erase(iter);
                break;
            }
            else if(*iter=="-n"||*iter=="--namespace")
            {
                if(type==Simple)
                    break;
                else
                {
                    std::cout<<ARGS_ERROR_STR<<std::endl;
                    return;
                }
            }
            else
            {
                std::cout<<ARGS_ERROR_STR<<std::endl;
                return;
            }
        }
    }
    if(type==Simple)
    {
        std::string namespaceName;
        for(auto iter=parameters.begin();iter!=parameters.end();iter++)
        {
            if((*iter)[0]=='-')
            {
                if(*iter=="-n"||*iter=="--namespace")
                {
                    parameters.erase(iter);
                    if(iter!=parameters.end())
                    {
                        namespaceName=*iter;
                        parameters.erase(iter);
                        break;
                    }
                    else
                    {
                        std::cout<<"please enter namespace"<<std::endl;
                        return;
                    }
                }else
                {
                    std::cout<<ARGS_ERROR_STR<<std::endl;
                    return;
                }
            }

        }
        createSimpleController(parameters,namespaceName);
    }
    else
        createApiController(parameters);
}

void create_controller::createSimpleController(std::vector<std::string> &ctlNames,const std::string &namespaceName)
{
    for(auto iter=ctlNames.begin();iter!=ctlNames.end();iter++)
    {
        if ((*iter)[0] == '-')
        {
            std::cout<<ARGS_ERROR_STR<<std::endl;
            return;
        }
    }
    for(auto ctlName:ctlNames)
    {
        createSimpleController(ctlName,namespaceName);
    }
}
void create_controller::createSimpleController(const std::string &ctlName,const std::string &namespaceName)
{
    std::cout<<"create simple controller:"<<namespaceName<<(namespaceName==""?"":"::")<<ctlName<<std::endl;
    std::string headFileName=ctlName+".h";
    std::string sourceFilename=ctlName+".cc";
    std::ofstream oHeadFile(headFileName.c_str(),std::ofstream::out);
    std::ofstream oSourceFile(sourceFilename.c_str(),std::ofstream::out);
    if(!oHeadFile||!oSourceFile)
        return;
    newSimpleControllerHeaderFile(oHeadFile,ctlName,namespaceName);
    newSimpleControllerSourceFile(oSourceFile,ctlName,namespaceName);
}
void create_controller::newSimpleControllerHeaderFile(std::ofstream &file,const std::string &ctlName,const std::string &namespaceName)
{
    file<<"#pragma once\n";
    file<<"#include <drogon/HttpSimpleController.h>\n";
    file<<"using namespace drogon;\n";
    std::string indent="";
    if(namespaceName!="") {
        file << "namespace " << namespaceName << "{\n";
        indent="    ";
    }
    file<<indent<<"class "<<ctlName<<":public drogon::HttpSimpleController<"<<ctlName<<">\n";
    file<<indent<<"{\n";
    file<<indent<<"public:\n";
        //TestController(){}
    file<<indent<<"    virtual void asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)override;\n";

    file<<indent<<"    PATH_LIST_BEGIN\n";
    file<<indent<<"    //list path definations here;\n";
    file<<indent<<"    //PATH_ADD(\"/path\",\"filter1\",\"filter2\",...);\n";
    file<<indent<<"    PATH_LIST_END\n";
    file<<indent<<"};\n";
    if(namespaceName!="")
        file<<"}\n";
}
void create_controller::newSimpleControllerSourceFile(std::ofstream &file,const std::string &ctlName,const std::string &namespaceName)
{
    file<<"#include \""<<ctlName<<".h\"\n";
    if(namespaceName!="")
        file<<"using namespace "<<namespaceName<<";\n";
    file<<"void "<<ctlName<<"::asyncHandleHttpRequest(const HttpRequest& req,std::function<void (HttpResponse &)>callback)\n";
    file<<"{\n";
    file<<"    //write your application logic here\n";
    file<<"}";
}
void create_controller::createApiController(std::vector<std::string> &apiPaths)
{

}