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
#include <algorithm>

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

inline std::string nameTransform(const std::string &origName, bool isType)
{
    auto str = origName;
    std::transform(str.begin(), str.end(), str.begin(), tolower);
    std::string::size_type startPos = 0;
    std::string::size_type pos;
    std::string ret;
    do
    {
        pos = str.find("_", startPos);
        if (pos == std::string::npos)
        {
            pos = str.find(".", startPos);
        }
        if (pos != std::string::npos)
            ret += str.substr(startPos, pos - startPos);
        else
        {
            ret += str.substr(startPos);
            break;
        }
        while (str[pos] == '_' || str[pos] == '.')
            pos++;
        if (str[pos] >= 'a' && str[pos] <= 'z')
            str[pos] += ('A' - 'a');
        startPos = pos;
    } while (1);
    if (isType && ret[0] >= 'a' && ret[0] <= 'z')
        ret[0] += ('A' - 'a');
    return ret;
}
class PivotTable
{
  public:
    PivotTable() = default;
    PivotTable(const Json::Value &json)
    {
        _tableName = json.get("table_name", "").asString();
        if (_tableName.empty())
        {
            throw std::runtime_error("table_name can't be empty");
        }
        _originalKey = json.get("original_key", "").asString();
        if (_originalKey.empty())
        {
            throw std::runtime_error("original_key can't be empty");
        }
        _targetKey = json.get("target_key", "").asString();
        if (_targetKey.empty())
        {
            throw std::runtime_error("target_key can't be empty");
        }
    }
    PivotTable reverse() const
    {
        PivotTable pivot;
        pivot._tableName = _tableName;
        pivot._originalKey = _targetKey;
        pivot._targetKey = _originalKey;
        return pivot;
    }
    const std::string &tableName() const
    {
        return _tableName;
    }
    const std::string &originalKey() const
    {
        return _originalKey;
    }
    const std::string &targetKey() const
    {
        return _targetKey;
    }

  private:
    std::string _tableName;
    std::string _originalKey;
    std::string _targetKey;
};
class Relationship
{
  public:
    enum class Type
    {
        HasOne,
        HasMany,
        ManyToMany
    };
    Relationship(const Json::Value &relationship)
    {
        auto type = relationship.get("type", "has one").asString();
        if (type == "has one")
        {
            _type = Relationship::Type::HasOne;
        }
        else if (type == "has many")
        {
            _type = Relationship::Type::HasMany;
        }
        else if (type == "many to many")
        {
            _type = Relationship::Type::ManyToMany;
        }
        else
        {
            char message[128];
            sprintf(message, "Invalid relationship type: %s", type.data());
            throw std::runtime_error(message);
        }
        _originalTableName =
            relationship.get("original_table_name", "").asString();
        if (_originalTableName.empty())
        {
            throw std::runtime_error("original_table_name can't be empty");
        }
        _originalKey = relationship.get("original_key", "").asString();
        if (_originalKey.empty())
        {
            throw std::runtime_error("original_key can't be empty");
        }
        _originalTableAlias =
            relationship.get("original_table_alias", "").asString();
        _targetTableName = relationship.get("target_table_name", "").asString();
        if (_targetTableName.empty())
        {
            throw std::runtime_error("target_table_name can't be empty");
        }
        _targetKey = relationship.get("target_key", "").asString();
        if (_targetKey.empty())
        {
            throw std::runtime_error("target_key can't be empty");
        }
        _targetTableAlias =
            relationship.get("target_table_alias", "").asString();
        _enableReverse = relationship.get("enable_reverse", false).asBool();
        if (_type == Type::ManyToMany)
        {
            auto &pivot = relationship["pivot_table"];
            if (pivot.isNull())
            {
                throw std::runtime_error(
                    "ManyToMany relationship needs a pivot table");
            }
            _pivotTable = PivotTable(pivot);
        }
    }
    Relationship() = default;
    Relationship reverse() const
    {
        Relationship r;
        if (_type == Type::HasMany)
        {
            r._type = Type::HasOne;
        }
        else
        {
            r._type = _type;
        }
        r._originalTableName = _targetTableName;
        r._originalTableAlias = _targetTableAlias;
        r._originalKey = _targetKey;
        r._targetTableName = _originalTableName;
        r._targetTableAlias = _originalTableAlias;
        r._targetKey = _originalKey;
        r._enableReverse = _enableReverse;
        r._pivotTable = _pivotTable.reverse();
        return r;
    }
    Type type() const
    {
        return _type;
    }
    bool enableReverse() const
    {
        return _enableReverse;
    }
    const std::string &originalTableName() const
    {
        return _originalTableName;
    }
    const std::string &originalTableAlias() const
    {
        return _originalTableAlias;
    }
    const std::string &originalKey() const
    {
        return _originalKey;
    }
    const std::string &targetTableName() const
    {
        return _targetTableName;
    }
    const std::string &targetTableAlias() const
    {
        return _targetTableAlias;
    }
    const std::string &targetKey() const
    {
        return _targetKey;
    }
    const PivotTable &pivotTable() const
    {
        return _pivotTable;
    }

  private:
    Type _type = Type::HasOne;
    std::string _originalTableName;
    std::string _originalTableAlias;
    std::string _targetTableName;
    std::string _targetTableAlias;
    std::string _originalKey;
    std::string _targetKey;
    bool _enableReverse = false;
    PivotTable _pivotTable;
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
                                const Json::Value &restfulApiConfig,
                                const std::vector<Relationship> &relationships);
    void createModelFromPG(
        const std::string &path,
        const DbClientPtr &client,
        const std::string &schema,
        const Json::Value &restfulApiConfig,
        std::map<std::string, std::vector<Relationship>> &relationships);
#endif
#if USE_MYSQL
    void createModelClassFromMysql(
        const std::string &path,
        const DbClientPtr &client,
        const std::string &tableName,
        const Json::Value &restfulApiConfig,
        const std::vector<Relationship> &relationships);
    void createModelFromMysql(
        const std::string &path,
        const DbClientPtr &client,
        const Json::Value &restfulApiConfig,
        std::map<std::string, std::vector<Relationship>> &relationships);
#endif
#if USE_SQLITE3
    void createModelClassFromSqlite3(
        const std::string &path,
        const DbClientPtr &client,
        const std::string &tableName,
        const Json::Value &restfulApiConfig,
        const std::vector<Relationship> &relationships);
    void createModelFromSqlite3(
        const std::string &path,
        const DbClientPtr &client,
        const Json::Value &restfulApiConfig,
        std::map<std::string, std::vector<Relationship>> &relationships);
#endif
    void createRestfulAPIController(const DrTemplateData &tableInfo,
                                    const Json::Value &restfulApiConfig);
    std::string _dbname;
    bool _forceOverwrite = false;
};
}  // namespace drogon_ctl
