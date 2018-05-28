#include "CommandHandler.h"
#include <drogon/DrClassMap.h>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
using namespace drogon;
int main(int argc, char* argv[])
{
    if(argc<2)
    {
        std::cout<<"usage:"<<std::endl;
        return 0;
    }
    std::string command=argv[1];

    std::string handlerName=std::string("drogon_ctl::").append(command);

    //new controller
    auto obj=std::shared_ptr<DrObjectBase>(drogon::DrClassMap::newObject(handlerName));
    if(obj)
    {
        auto ctl=std::dynamic_pointer_cast<CommandHandler>(obj);
        if(ctl)
        {
            std::vector<std::string> args;
            for(int i=2;i<argc;i++)
            {
                args.push_back(argv[i]);
            }
            ctl->handleCommand(args);
        }
        else
        {
            std::cout<<"command not found!"<<std::endl;
        }
    }
    else
    {
        std::cout<<"command error!"<<std::endl;
    }
    return 0;
}