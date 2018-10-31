/**
 *
 *  create_model.h
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

struct ColumnInfo
{
  std::string _colName;
  std::string _colValName;
  std::string _colTypeName;
  std::string _colType;
  ssize_t _colLength = -1;
  bool _isAutoVal = false;
  bool _isPrimaryKey = false;
  bool _notNull = false;
  bool _hasDefaultVal = false;
};

class create_model : public DrObject<create_model>, public CommandHandler
{
public:
  virtual void handleCommand(std::vector<std::string> &parameters) override;
  virtual std::string script() override { return "create Model classes files"; }

protected:
  void createModel(const std::string &path);
  void createModel(const std::string &path, const Json::Value &config);
#if USE_POSTGRESQL
  void createModelClassFromPG(const std::string &path, PgClient &client, const std::string &tableName);
  void createModelFromPG(const std::string &path, PgClient &client);
#endif
  std::string _dbname;
};
} // namespace drogon_ctl
