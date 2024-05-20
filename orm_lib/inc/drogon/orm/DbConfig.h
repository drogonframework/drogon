#pragma once

#include <variant>
#include "config/PostgresConfig.h"
#include "config/MysqlConfig.h"
#include "config/Sqlite3Config.h"

namespace drogon::orm
{
using DbConfig = std::variant<PostgresConfig, MysqlConfig, Sqlite3Config>;
}
