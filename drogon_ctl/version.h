#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;
namespace drogon_ctl
{
    class version:public DrObject<version>,public CommandHandler
    {
    public:
        virtual void handleCommand(std::vector<std::string> &parameters) override;
        std::string script(){return "display version of this tool";}
        version(){}
    };
}
