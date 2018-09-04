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
    class create_project:public DrObject<create_project>,public CommandHandler
    {
    public:
        virtual void handleCommand(std::vector<std::string> &parameters) override;
        virtual std::string script() override {return "create a project";}

    protected:
        std::string _outputPath=".";
        void createProject(const std::string &projectName);
    };
}