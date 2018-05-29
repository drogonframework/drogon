#pragma once

#include <drogon/DrObject.h>
#include <vector>
#include <string>

class CommandHandler:public virtual drogon::DrObjectBase {
public:

    virtual void handleCommand(std::vector<std::string> &parameters)=0;
    virtual bool isTopCommand(){return false;}
    virtual std::string script(){return "";}
    virtual ~CommandHandler(){}
};