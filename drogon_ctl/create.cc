/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#include "create.h"
#include "cmd.h"
#include <drogon/DrClassMap.h>
#include <iostream>
#include <memory>
using namespace drogon_ctl;
std::string create::detail()
{
    return "Use create command to create some source files of drogon webapp\n"
           "Usage:drogon_ctl create <view|controller> [-options] <object name>\n"
           "drogon_ctl create view <csp file name> //create HttpView source files from csp file\n"
           "drogon_ctl create controller [-s] [-n <namespace>] <class_name> //"
           "create HttpSimpleController source files\n"
           "drogon_ctl create controller -h <[namespace::]class_name> //"
           "create HttpController source files\n"
           "drogon_ctl create controller -w [-n <namespace>] <class_name> //"
           "create WebSocketController source files\n"
           "drogon_ctl create project <project_name> //"
           "create a project named project_name\n"
           "drogon_ctl create filter <class_name> //"
           "create a filter named class_name\n";
}

void create::handleCommand(std::vector<std::string> &parameters)
{
    //std::cout<<"create!"<<std::endl;
    auto createObjName = parameters[0];
    createObjName = std::string("create_") + createObjName;
    parameters[0] = createObjName;
    exeCommand(parameters);
}