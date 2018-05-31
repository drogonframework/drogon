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


void create::handleCommand(std::vector<std::string> &parameters)
{
    //std::cout<<"create!"<<std::endl;
    auto createObjName=parameters[0];
    createObjName=std::string("create_")+createObjName;
    parameters[0]=createObjName;
    exeCommand(parameters);
}