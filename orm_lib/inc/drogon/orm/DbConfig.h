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
#include <drogon/orm/config/PostgresConfig.h>
#include <drogon/orm/config/MysqlConfig.h>
#include <drogon/orm/config/Sqlite3Config.h>

namespace drogon::orm
{
using DbConfig = std::variant<PostgresConfig, MysqlConfig, Sqlite3Config>;
}
