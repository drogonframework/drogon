#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;
namespace drogon_ctl
{
    class version:public DrObject<version>,public CommandHandler
    {
    public:
        void handleCommand(const std::vector<std::string> &parameters);
        version(){}
    };
}
