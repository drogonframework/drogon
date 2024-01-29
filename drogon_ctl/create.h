/**
 *
 *  create.h
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
class create : public DrObject<create>, public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;

    std::string script() override
    {
        return "create some source files(Use 'drogon_ctl help create' for more "
               "information)";
    }

    bool isTopCommand() override
    {
        return true;
    }

    std::string detail() override;
};
}  // namespace drogon_ctl
