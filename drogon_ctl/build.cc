/**
 *
 *  build.cc
 *
 *  Copyright 2026, Drogon authors.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "build.h"
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace drogon_ctl
{
namespace project_build
{
int runCommand(const std::string &cmd)
{
    std::cout << "==> " << cmd << std::endl;
    return std::system(cmd.c_str());
}

bool ensureCmakeProject()
{
    if (!fs::exists("CMakeLists.txt"))
    {
        std::cerr << "No CMakeLists.txt in the current directory. "
                     "Run this from a Drogon project root "
                     "(e.g. after 'drogon_ctl create project <name>')."
                  << std::endl;
        return false;
    }
    return true;
}

std::string projectName()
{
    std::ifstream in("CMakeLists.txt");
    if (!in)
        return fs::current_path().filename().string();

    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    // Match: project(Name ...) or project(Name)
    static const std::regex re(
        R"(project\s*\(\s*([A-Za-z0-9_.-]+))",
        std::regex::icase);
    std::smatch m;
    if (std::regex_search(content, m, re) && m.size() > 1)
        return m[1].str();
    return fs::current_path().filename().string();
}

bool configureAndBuild(const std::vector<std::string> &extraCmakeArgs)
{
    if (!ensureCmakeProject())
        return false;

    std::error_code ec;
    fs::create_directories("build", ec);
    if (ec)
    {
        std::cerr << "Failed to create build/: " << ec.message() << std::endl;
        return false;
    }

    std::ostringstream cfg;
    cfg << "cmake -S . -B build";
    for (const auto &arg : extraCmakeArgs)
    {
        cfg << ' ' << arg;
    }
    if (runCommand(cfg.str()) != 0)
    {
        std::cerr << "cmake configure failed" << std::endl;
        return false;
    }
    if (runCommand("cmake --build build") != 0)
    {
        std::cerr << "cmake build failed" << std::endl;
        return false;
    }
    return true;
}

static bool isExecutableFile(const fs::path &p)
{
    if (!fs::is_regular_file(p))
        return false;
#ifdef _WIN32
    auto ext = p.extension().string();
    for (auto &c : ext)
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return ext == ".exe";
#else
    auto perms = fs::status(p).permissions();
    return (perms & fs::perms::owner_exec) != fs::perms::none ||
           (perms & fs::perms::group_exec) != fs::perms::none ||
           (perms & fs::perms::others_exec) != fs::perms::none;
#endif
}

std::string findBuiltExecutable(const std::string &name)
{
#ifdef _WIN32
    const std::string exeName = name + ".exe";
#else
    const std::string exeName = name;
#endif
    const fs::path candidates[] = {
        fs::path("build") / exeName,
        fs::path("build") / "Debug" / exeName,
        fs::path("build") / "Release" / exeName,
        fs::path("build") / "RelWithDebInfo" / exeName,
        fs::path("build") / "MinSizeRel" / exeName,
    };
    for (const auto &c : candidates)
    {
        if (isExecutableFile(c))
            return c.string();
    }

    // Fallback: first matching executable under build/
    if (fs::exists("build"))
    {
        for (auto it = fs::recursive_directory_iterator("build");
             it != fs::recursive_directory_iterator();
             ++it)
        {
            // Skip nested cmake/compiler dirs that are huge
            if (it->is_directory())
            {
                auto dirName = it->path().filename().string();
                if (dirName == "CMakeFiles" || dirName == ".cmake" ||
                    dirName == "Testing")
                {
                    it.disable_recursion_pending();
                    continue;
                }
            }
            if (!it->is_regular_file())
                continue;
            if (it->path().filename().string() == exeName &&
                isExecutableFile(it->path()))
            {
                return it->path().string();
            }
        }
    }
    return {};
}

}  // namespace project_build

void build::handleCommand(std::vector<std::string> &parameters)
{
    std::vector<std::string> cmakeArgs;
    for (const auto &p : parameters)
    {
        if (p == "--")
            continue;
        cmakeArgs.push_back(p);
    }
    if (!project_build::configureAndBuild(cmakeArgs))
        exit(1);
    std::cout << "Build succeeded." << std::endl;
}

std::string build::detail()
{
    return R"(Use drogon_ctl build to configure and compile the Drogon project
in the current working directory.

  drogon_ctl build
  drogon_ctl build -- -DCMAKE_BUILD_TYPE=Release

Creates ./build if missing, runs `cmake -S . -B build`, then
`cmake --build build`. Extra arguments are forwarded to the configure step.
)";
}

}  // namespace drogon_ctl

template class drogon::DrObject<drogon_ctl::build>;
