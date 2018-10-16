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
 *
 */

#include "version.h"
#include <drogon/version.h>
#include <drogon/config.h>
#include <iostream>

using namespace drogon_ctl;
static const char banner[] = "     _                             \n"
                             "  __| |_ __ ___   __ _  ___  _ __  \n"
                             " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
                             "| (_| | | | (_) | (_| | (_) | | | |\n"
                             " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
                             "                 |___/             \n";

void version::handleCommand(std::vector<std::string> &parameters)
{
    std::cout << banner << std::endl;
    std::cout << "drogon ctl tools" << std::endl;
    std::cout << "version:" << VERSION << std::endl;
    std::cout << "git commit:" << VERSION_MD5 << std::endl;
    std::cout << "compile config:" << compileFlags <<" "<< includeDirs << std::endl;
}