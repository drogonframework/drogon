/**
 *
 *  cmd.cc
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

#include "cmd.h"
#include "CommandHandler.h"
#include <drogon/DrClassMap.h>
#include <memory>
using namespace drogon;

void exeCommand(std::vector<std::string> &parameters)
{
    if (parameters.empty())
    {
        std::cout << "incomplete command!use help command to get usage!"
                  << std::endl;
        return;
    }
    std::string command = parameters[0];

    std::string handlerName = std::string("drogon_ctl::").append(command);

    parameters.erase(parameters.begin());

    // new command handler to do cmd
    auto obj = std::shared_ptr<DrObjectBase>(
        drogon::DrClassMap::newObject(handlerName));
    if (obj)
    {
        auto ctl = std::dynamic_pointer_cast<CommandHandler>(obj);
        if (ctl)
        {
            ctl->handleCommand(parameters);
        }
        else
        {
            std::cout << "command not found!use help command to get usage!"
                      << std::endl;
        }
    }
    else
    {
        std::cout << "command error!use help command to get usage!"
                  << std::endl;
    }
}
