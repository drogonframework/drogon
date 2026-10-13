/**
 *
 *  build.h
 *
 *  Copyright 2026, Drogon authors.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include "CommandHandler.h"
#include <drogon/DrObject.h>
#include <string>
#include <vector>

using namespace drogon;

namespace drogon_ctl
{
/// Shared helpers used by `build` and `run`.
namespace project_build
{
bool ensureCmakeProject();
std::string projectName();
bool configureAndBuild(const std::vector<std::string> &extraCmakeArgs);
std::string findBuiltExecutable(const std::string &name);
int runCommand(const std::string &cmd);
}  // namespace project_build

class build : public DrObject<build>, public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;

    std::string script() override
    {
        return "configure and build the current Drogon project "
               "(Use 'drogon_ctl help build' for more information)";
    }

    bool isTopCommand() override
    {
        return true;
    }

    std::string detail() override;
};
}  // namespace drogon_ctl
