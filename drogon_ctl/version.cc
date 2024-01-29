/**
 *
 *  version.cc
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

#include "version.h"
#include <drogon/config.h>
#include <drogon/version.h>
#include <drogon/utils/Utilities.h>
#include <trantor/net/Resolver.h>
#include <trantor/utils/Utilities.h>
#include <iostream>

using namespace drogon_ctl;
static const char banner[] =
    R"(     _
  __| |_ __ ___   __ _  ___  _ __
 / _` | '__/ _ \ / _` |/ _ \| '_ \
| (_| | | | (_) | (_| | (_) | | | |
 \__,_|_|  \___/ \__, |\___/|_| |_|
                 |___/
)";

void version::handleCommand(std::vector<std::string> &parameters)
{
    const auto tlsBackend = trantor::utils::tlsBackend();
    const bool tlsSupported = drogon::utils::supportsTls();
    std::cout << banner << std::endl;
    std::cout << "A utility for drogon" << std::endl;
    std::cout << "Version: " << DROGON_VERSION << std::endl;
    std::cout << "Git commit: " << DROGON_VERSION_SHA1 << std::endl;
    std::cout << "Compilation: \n  Compiler: " << COMPILER_COMMAND
              << "\n  Compiler ID: " << COMPILER_ID
              << "\n  Compilation flags: " << COMPILATION_FLAGS
              << INCLUDING_DIRS << std::endl;
    std::cout << "Libraries: \n  postgresql: "
              << (USE_POSTGRESQL ? "yes" : "no") << "  (pipeline mode: "
              << (LIBPQ_SUPPORTS_BATCH_MODE ? "yes)\n" : "no)\n")
              << "  mariadb: " << (USE_MYSQL ? "yes\n" : "no\n")
              << "  sqlite3: " << (USE_SQLITE3 ? "yes\n" : "no\n");
    std::cout << "  ssl/tls backend: " << tlsBackend << "\n";
#ifdef USE_BROTLI
    std::cout << "  brotli: yes\n";
#else
    std::cout << "  brotli: no\n";
#endif
#ifdef USE_REDIS
    std::cout << "  hiredis: yes\n";
#else
    std::cout << "  hiredis: no\n";
#endif
    std::cout << "  c-ares: "
              << (trantor::Resolver::isCAresUsed() ? "yes\n" : "no\n");
#ifdef HAS_YAML_CPP
    std::cout << "  yaml-cpp: yes\n";
#else
    std::cout << "  yaml-cpp: no\n";
#endif
}
