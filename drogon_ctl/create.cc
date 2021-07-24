/**
 *
 *  @file create.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
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
    return "Use create command to create some source files of drogon webapp\n\n"
           "Usage:drogon_ctl create <view|controller|filter|project|model> "
           "[-options] <object name>\n\n"
           "drogon_ctl create view <csp file name> [-o <output path>] [-n "
           "<namespace>]|[--path-to-namespace]//create HttpView source files "
           "from csp files\n\n"
           "drogon_ctl create controller [-s] <[namespace::]class_name> //"
           "create HttpSimpleController source files\n\n"
           "drogon_ctl create controller -h <[namespace::]class_name> //"
           "create HttpController source files\n\n"
           "drogon_ctl create controller -w <[namespace::]class_name> //"
           "create WebSocketController source files\n\n"
           "drogon_ctl create controller -r <[namespace::]class_name> "
           "[--resource=...]//"
           "create restful controller source files\n\n"
           "drogon_ctl create filter <[namespace::]class_name> //"
           "create a filter named class_name\n\n"
           "drogon_ctl create plugin <[namespace::]class_name> //"
           "create a plugin named class_name\n\n"
           "drogon_ctl create project <project_name> //"
           "create a project named project_name\n\n"
           "drogon_ctl create model <model_path> [--table=<table_name>] [-f]//"
           "create model classes in model_path\n\n"
           "drogon_ctl create swagger <swagger_path> //"
           "create swagger controller in swagger_path\n\n";
}

void create::handleCommand(std::vector<std::string> &parameters)
{
    // std::cout<<"create!"<<std::endl;
    auto createObjName = parameters[0];
    createObjName = std::string("create_") + createObjName;
    parameters[0] = createObjName;
    exeCommand(parameters);
}
