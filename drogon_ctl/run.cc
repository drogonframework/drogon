/**
 *
 *  run.cc
 *
 *  Copyright 2026, Drogon authors.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "run.h"
#include "build.h"
#include <iostream>
#include <sstream>

using namespace drogon_ctl;

void run::handleCommand(std::vector<std::string> &parameters)
{
    // Arguments before "--" go to cmake configure; after "--" go to the app.
    std::vector<std::string> cmakeArgs;
    std::vector<std::string> appArgs;
    bool pastSeparator = false;
    for (const auto &p : parameters)
    {
        if (!pastSeparator && p == "--")
        {
            pastSeparator = true;
            continue;
        }
        if (pastSeparator)
            appArgs.push_back(p);
        else
            cmakeArgs.push_back(p);
    }

    if (!project_build::configureAndBuild(cmakeArgs))
        exit(1);

    auto name = project_build::projectName();
    auto exe = project_build::findBuiltExecutable(name);
    if (exe.empty())
    {
        std::cerr << "Could not find built executable for project '" << name
                  << "' under ./build. Build may have produced a different "
                     "target name."
                  << std::endl;
        exit(1);
    }

    std::ostringstream cmd;
#ifdef _WIN32
    cmd << '"' << exe << '"';
#else
    cmd << exe;
#endif
    for (const auto &a : appArgs)
    {
        cmd << ' ' << a;
    }
    int rc = project_build::runCommand(cmd.str());
    if (rc != 0)
        exit(rc);
}

std::string run::detail()
{
    return R"(Use drogon_ctl run to build the current Drogon project (if needed)
and execute the generated binary.

  drogon_ctl run
  drogon_ctl run -- -DCMAKE_BUILD_TYPE=Release
  drogon_ctl run -- -DCMAKE_BUILD_TYPE=Release -- arg1 arg2

The project name is taken from `project(...)` in CMakeLists.txt.
Arguments before `--` are passed to cmake configure; arguments after
`--` are passed to the application.
)";
}

template class drogon::DrObject<drogon_ctl::run>;
