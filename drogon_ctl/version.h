/**
 *
 *  version.h
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

#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;

namespace drogon_ctl
{
class version : public DrObject<version>, public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;

    std::string script() override
    {
        return "display version of this tool";
    }

    bool isTopCommand() override
    {
        return true;
    }

    version()
    {
    }
};
}  // namespace drogon_ctl
