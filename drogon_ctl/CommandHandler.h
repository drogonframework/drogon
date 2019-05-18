/**
 *
 *  CommandHandler.h
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
#include <vector>
#include <string>

class CommandHandler : public virtual drogon::DrObjectBase
{
  public:
    virtual void handleCommand(std::vector<std::string> &parameters) = 0;
    virtual bool isTopCommand()
    {
        return false;
    }
    virtual std::string script()
    {
        return "";
    }
    virtual std::string detail()
    {
        return "";
    }
    virtual ~CommandHandler()
    {
    }
};
