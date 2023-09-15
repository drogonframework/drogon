/**
 *
 *  @file create_view.h
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

#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;

namespace drogon_ctl
{
class create_view : public DrObject<create_view>, public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;

    std::string script() override
    {
        return "create view class files";
    }

  protected:
    std::string outputPath_{"."};
    std::vector<std::string> namespaces_;
    bool pathToNamespaceFlag_{false};
    void createViewFiles(std::vector<std::string> &cspFileNames);
    int createViewFile(const std::string &script_filename);
    void newViewHeaderFile(std::ofstream &file, const std::string &className);
    void newViewSourceFile(std::ofstream &file,
                           const std::string &className,
                           const std::string &namespacePrefix,
                           std::ifstream &infile);
};
}  // namespace drogon_ctl
