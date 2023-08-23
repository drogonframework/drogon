/**
 *
 *  create_controller.h
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

#include <drogon/DrObject.h>
#include <drogon/DrTemplateBase.h>
#include "CommandHandler.h"
using namespace drogon;

namespace drogon_ctl
{
class create_controller : public DrObject<create_controller>,
                          public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;

    std::string script() override
    {
        return "create controller files";
    }

  protected:
    enum ControllerType
    {
        Simple = 0,
        Http,
        WebSocket,
        Restful
    };

    void createController(std::vector<std::string> &httpClasses,
                          ControllerType type);
    void createController(const std::string &className, ControllerType type);
    void createARestfulController(const std::string &className,
                                  const std::string &resource);

    void newSimpleControllerHeaderFile(std::ofstream &file,
                                       const std::string &className);
    void newSimpleControllerSourceFile(std::ofstream &file,
                                       const std::string &className,
                                       const std::string &filename);

    void newWebsockControllerHeaderFile(std::ofstream &file,
                                        const std::string &className);
    void newWebsockControllerSourceFile(std::ofstream &file,
                                        const std::string &className,
                                        const std::string &filename);

    void newHttpControllerHeaderFile(std::ofstream &file,
                                     const std::string &className);
    void newHttpControllerSourceFile(std::ofstream &file,
                                     const std::string &className,
                                     const std::string &filename);
};
}  // namespace drogon_ctl
