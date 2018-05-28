#include <drogon/version.h>
#include <string>
#include <vector>
#include <iostream>
static const char banner[]="     _                             \n"
                           "  __| |_ __ ___   __ _  ___  _ __  \n"
                           " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
                           "| (_| | | | (_) | (_| | (_) | | | |\n"
                           " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
                           "                 |___/             \n";

int main(int argc, char* argv[])
{
    if(argc<2)
    {
        std::cout<<"usage:"<<std::endl;
        return 0;
    }
    std::string command=argv[1];
    if(command=="version")
    {
        std::cout<<banner<<std::endl;
        std::cout<<"drogon ctl tools"<<std::endl;
        std::cout<<"version:"<<VERSION<<std::endl;
        std::cout<<"git commit:"<<VERSION_MD5<<std::endl;
    }
    return 0;
}