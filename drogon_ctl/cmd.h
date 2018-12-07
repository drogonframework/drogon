/**
 *
 *  cmd.h
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

#pragma once

#include <vector>
#include <string>
#include <iostream>
#define ARGS_ERROR_STR "args error!use help command to get usage!"
void exeCommand(std::vector<std::string> &parameters);
