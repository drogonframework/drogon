/**
 *
 *  create_swagger.h
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
class create_swagger : public DrObject<create_swagger>, public CommandHandler
{
  public:
    virtual void handleCommand(std::vector<std::string> &parameters) override;
    virtual std::string script() override
    {
        return "create swagger docs controller";
    }
};
}  // namespace drogon_ctl