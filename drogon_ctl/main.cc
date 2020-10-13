/**
 *
 *  @file main.cc
 *  @author An Tao
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
#include <string>
#include <vector>
#include <iostream>

int main(int argc, char *argv[])
{
    std::vector<std::string> args;
    if (argc < 2)
    {
        args = {"help"};
        exeCommand(args);
        return 0;
    }
    for (int i = 1; i < argc; ++i)
    {
        args.push_back(argv[i]);
    }
    if (args.size() > 0)
    {
        auto &arg = args[0];
        if (arg == "-h" || arg == "--help")
        {
            arg = "help";
        }
        else if (arg == "-v" || arg == "--version")
        {
            arg = "version";
        }
    }

    exeCommand(args);

    return 0;
}
