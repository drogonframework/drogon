#include "help.h"
#include <drogon/DrClassMap.h>
#include <iostream>
#include <memory>
using namespace drogon_ctl;
void help::handleCommand(const std::vector<std::string> &parameters)
{
    std::cout<<"usage: drogon_ctl <command> [<args>]"<<std::endl;
    std::cout<<"commands list:"<<std::endl;
    for(auto className:drogon::DrClassMap::getAllClassName())
    {
        auto classPtr=std::shared_ptr<DrObjectBase>(drogon::DrClassMap::newObject(className));
        if(classPtr)
        {
            auto cmdHdlPtr=std::dynamic_pointer_cast<CommandHandler>(classPtr);
            if(cmdHdlPtr)
            {
                auto pos=className.rfind("::");
                if(pos!=std::string::npos)
                {
                    className=className.substr(pos+2);
                }
                while(className.length()<24)
                    className.append(" ");
                std::cout<<className<<cmdHdlPtr->script()<<std::endl;
            }
        }
    }
}