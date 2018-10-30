/**
 *
 *  create_dao.h
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

#include <drogon/config.h>
#include <json/json.h>
#if USE_POSTGRESQL
#include <drogon/orm/PgClient.h>
#endif
#include <drogon/DrObject.h>
#include "CommandHandler.h"
#include <string>

using namespace drogon;
using namespace drogon::orm;

namespace drogon_ctl
{
class create_dao : public DrObject<create_dao>, public CommandHandler
{
public:
  virtual void handleCommand(std::vector<std::string> &parameters) override;
  virtual std::string script() override { return "create DAO classes files"; }

protected:
  void createDAO(const std::string &path);
  void createDAO(const std::string &path, const Json::Value &config);
#if USE_POSTGRESQL
  void createDAOClassFromPG(const std::string &path, PgClient &client, const std::string &tableName);
  void createDAOFromPG(const std::string &path, PgClient &client);
#endif
  std::string _dbname;
};
} // namespace drogon_ctl