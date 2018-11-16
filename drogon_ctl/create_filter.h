/**
 *
 *  create_filter.h
 *  An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;
namespace drogon_ctl
{
class create_filter : public DrObject<create_filter>, public CommandHandler
{
  public:
    virtual void handleCommand(std::vector<std::string> &parameters) override;
    virtual std::string script() override { return "create filter class files"; }

  protected:
    std::string _outputPath = ".";
    void createViewFiles(std::vector<std::string> &cspFileNames);
    int createViewFile(const std::string &script_filename);
    void newViewHeaderFile(std::ofstream &file, const std::string &className);
    void newViewSourceFile(std::ofstream &file, const std::string &className, std::ifstream &infile);
};
} // namespace drogon_ctl
