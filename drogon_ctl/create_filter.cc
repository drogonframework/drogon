/**
 *
 *  create_filter.cc
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

#include "create_filter.h"
#include <string>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

#include <drogon/DrTemplateBase.h>
using namespace drogon_ctl;

static void createFilterHeaderFile(const std::string &className)
{
    std::ofstream file(className + ".h", std::ofstream::out);
    auto templ = drogon::DrTemplateBase::newTemplate("filter_h");
    HttpViewData data;
    data.insert("className", className);
    file << templ->genText(data);
}

static void createFilterSourceFile(const std::string &className)
{
    std::ofstream file(className + ".cc", std::ofstream::out);
    auto templ = drogon::DrTemplateBase::newTemplate("filter_cc");
    HttpViewData data;
    data.insert("className", className);
    file << templ->genText(data);
}
void create_filter::handleCommand(std::vector<std::string> &parameters)
{
    if (parameters.size() < 1)
    {
        std::cout << "Invalid parameters!" << std::endl;
    }
    auto className = parameters[0];
    createFilterHeaderFile(className);
    createFilterSourceFile(className);
}
