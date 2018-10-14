/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *  this is a drogon lib tool program
 *  run drogon_ctl help to get usage
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