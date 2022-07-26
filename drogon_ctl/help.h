/**
 *
 *  help.h
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
class help : public DrObject<help>, public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;
    std::string script() override
    {
        return "display this message";
    }
    bool isTopCommand() override
    {
        return true;
    }
};
}  // namespace drogon_ctl
