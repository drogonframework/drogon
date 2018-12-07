/**
 *
 *  main.cc
 *  An Tao
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
    if (argc < 2)
    {
        std::vector<std::string> args = {"help"};
        exeCommand(args);
        return 0;
    }
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++)
    {
        args.push_back(argv[i]);
    }

    exeCommand(args);

    return 0;
}
