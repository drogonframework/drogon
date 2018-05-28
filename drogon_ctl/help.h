#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;
namespace drogon_ctl
{
    class help:public DrObject<help>,public CommandHandler
    {
    public:
        void handleCommand(const std::vector<std::string> &parameters);
        std::string script(){return "display this message";}
    };
}
