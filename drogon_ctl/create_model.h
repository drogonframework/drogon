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

struct Relationship
{
  public:
    enum class Type
    {
        HasOne,
        HasMany,
        ManyToMany
    };

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
        return r;
    }

    void setOriginalTableName(const std::string &tableName)
    {
        _originalTableName = tableName;
        std::transform(_originalTableName.begin(),
                       _originalTableName.end(),
                       _originalTableName.begin(),
                       tolower);
    }
    void setOriginalTableAlias(const std::string &alias)
    {
        _originalTableAlias = alias;
    }
    void setTargetTableName(const std::string &tableName)
    {
        _targetTableName = tableName;
        std::transform(_targetTableName.begin(),
                       _targetTableName.end(),
                       _targetTableName.begin(),
                       tolower);
    }
    void setTargetTableAlias(const std::string &alias)
    {
        _targetTableAlias = alias;
    }
    void setOriginalKey(const std::string &key)
    {
        _originalKey = key;
    }
    void setTargetKey(const std::string &key)
    {
        _targetKey = key;
    }
    void setType(Type type)
    {
        _type = type;
    }
    void setEnableReverse(bool reverse)
    {
        _enableReverse = reverse;
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

  private:
    Type _type = Type::HasOne;
    std::string _originalTableName;
    std::string _originalTableAlias;
    std::string _targetTableName;
    std::string _targetTableAlias;
    std::string _originalKey;
    std::string _targetKey;
    bool _enableReverse = false;
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
