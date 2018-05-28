#pragma once

#include <drogon/DrObject.h>
#include <vector>
#include <string>

class CommandHandler:public virtual drogon::DrObjectBase {
public:

    virtual void handleCommand(const std::vector<std::string> &parameters)=0;
    virtual ~CommandHandler(){}
};