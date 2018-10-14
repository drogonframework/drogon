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
#include <vector>
#include <string>

class CommandHandler : public virtual drogon::DrObjectBase
{
  public:
    virtual void handleCommand(std::vector<std::string> &parameters) = 0;
    virtual bool isTopCommand() { return false; }
    virtual std::string script() { return ""; }
    virtual std::string detail() { return ""; }
    virtual ~CommandHandler() {}
};