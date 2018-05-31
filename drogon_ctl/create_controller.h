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

#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;
namespace drogon_ctl
{
    class create_controller:public DrObject<create_controller>,public CommandHandler
    {
    public:
        virtual void handleCommand(std::vector<std::string> &parameters) override;
        std::string script(){return "create controller files";}

    protected:
        enum ControllerType{
            Simple=0,
            API
        };
        void createSimpleController(std::vector<std::string> &ctlNames,const std::string &namespaceName="");
        void createSimpleController(const std::string &ctlName,const std::string &namespaceName="");
        void createApiController(std::vector<std::string> &apiPaths);
        void newSimpleControllerHeaderFile(std::ofstream &file,const std::string &ctlName,const std::string &namespaceName="");
        void newSimpleControllerSourceFile(std::ofstream &file,const std::string &ctlName,const std::string &namespaceName="");
    };
}
