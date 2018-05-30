
#include "cmd.h"

#include <string>
#include <vector>
#include <iostream>


int main(int argc, char* argv[])
{
    if(argc<2)
    {
        std::vector<std::string> args={"help"};
        exeCommand(args);
        return 0;
    }
    std::vector<std::string> args;
    for(int i=1;i<argc;i++)
    {
        args.push_back(argv[i]);
    }

    exeCommand(args);

    return 0;
}