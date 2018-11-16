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
    virtual void handleCommand(std::vector<std::string> &parameters) override;
    virtual std::string script() override { return "create controller or view class files"; }
    virtual bool isTopCommand() override { return true; }
    virtual std::string detail() override;
};
} // namespace drogon_ctl
