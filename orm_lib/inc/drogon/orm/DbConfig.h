/**
 *
 *  @file DbConfig.h
 *  @author Nitromelon
 *
 *  Copyright 2024, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <variant>
#include "config/PostgresConfig.h"
#include "config/MysqlConfig.h"
#include "config/Sqlite3Config.h"

namespace drogon::orm
{
using DbConfig = std::variant<PostgresConfig, MysqlConfig, Sqlite3Config>;
}
