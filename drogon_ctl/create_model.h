/**
 *
 *  create_model.h
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

#include <drogon/config.h>
#include <drogon/DrTemplateBase.h>
#include <json/json.h>
#include <drogon/orm/DbClient.h>
using namespace drogon::orm;
#include <drogon/DrObject.h>
#include "CommandHandler.h"
#include <string>

using namespace drogon;

namespace drogon_ctl
{
struct ColumnInfo
{
    std::string _colName;
    std::string _colValName;
    std::string _colTypeName;
    std::string _colType;
    std::string _colDatabaseType;
    std::string _dbType;
    ssize_t _colLength = 0;
    size_t _index = 0;
    bool _isAutoVal = false;
    bool _isPrimaryKey = false;
    bool _notNull = false;
    bool _hasDefaultVal = false;
};

class create_model : public DrObject<create_model>, public CommandHandler
{
  public:
    virtual void handleCommand(std::vector<std::string> &parameters) override;
    virtual std::string script() override
    {
        return "create Model classes files";
    }

  protected:
    void createModel(const std::string &path,
                     const std::string &singleModelName);
    void createModel(const std::string &path,
                     const Json::Value &config,
                     const std::string &singleModelName);
#if USE_POSTGRESQL
    void createModelClassFromPG(const std::string &path,
                                const DbClientPtr &client,
                                const std::string &tableName,
                                const std::string &schema,
                                const Json::Value &restfulApiConfig);
    void createModelFromPG(const std::string &path,
                           const DbClientPtr &client,
                           const std::string &schema,
                           const Json::Value &restfulApiConfig);
#endif
#if USE_MYSQL
    void createModelClassFromMysql(const std::string &path,
                                   const DbClientPtr &client,
                                   const std::string &tableName,
                                   const Json::Value &restfulApiConfig);
    void createModelFromMysql(const std::string &path,
                              const DbClientPtr &client,
                              const Json::Value &restfulApiConfig);
#endif
#if USE_SQLITE3
    void createModelClassFromSqlite3(const std::string &path,
                                     const DbClientPtr &client,
                                     const std::string &tableName,
                                     const Json::Value &restfulApiConfig);
    void createModelFromSqlite3(const std::string &path,
                                const DbClientPtr &client,
                                const Json::Value &restfulApiConfig);
#endif
    void createRestfulAPIController(const DrTemplateData &tableInfo,
                                    const Json::Value &restfulApiConfig);
    std::string _dbname;
    bool _forceOverwrite = false;
};
}  // namespace drogon_ctl
