/**
 *
 *  create_controller.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once

#include <drogon/DrObject.h>
#include "CommandHandler.h"
using namespace drogon;
namespace drogon_ctl
{
class create_controller : public DrObject<create_controller>, public CommandHandler
{
  public:
    virtual void handleCommand(std::vector<std::string> &parameters) override;
    virtual std::string script() override { return "create controller files"; }

  protected:
    enum ControllerType
    {
        Simple = 0,
        Http,
        WebSocket
    };
    void createSimpleController(std::vector<std::string> &ctlNames, const std::string &namespaceName = "");
    void createSimpleController(const std::string &ctlName, const std::string &namespaceName = "");
    void createWebsockController(std::vector<std::string> &ctlNames, const std::string &namespaceName = "");
    void createWebsockController(const std::string &ctlName, const std::string &namespaceName = "");

    void createHttpController(std::vector<std::string> &httpClasses);
    void createHttpController(const std::string &className);
    void newSimpleControllerHeaderFile(std::ofstream &file, const std::string &ctlName, const std::string &namespaceName = "");
    void newSimpleControllerSourceFile(std::ofstream &file, const std::string &ctlName, const std::string &namespaceName = "");
    void newWebsockControllerHeaderFile(std::ofstream &file, const std::string &ctlName, const std::string &namespaceName = "");
    void newWebsockControllerSourceFile(std::ofstream &file, const std::string &ctlName, const std::string &namespaceName = "");

    void newHttpControllerHeaderFile(std::ofstream &file, const std::string &className);
    void newHttpControllerSourceFile(std::ofstream &file, const std::string &className, const std::string &filename);
};
} // namespace drogon_ctl
