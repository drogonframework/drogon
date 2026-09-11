/**
 *
 *  run.h
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
class run : public DrObject<run>, public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;

    std::string script() override
    {
        return "build (if needed) and run the current Drogon project "
               "(Use 'drogon_ctl help run' for more information)";
    }

    bool isTopCommand() override
    {
        return true;
    }

    std::string detail() override;
};
}  // namespace drogon_ctl
