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
    std::string colName_;
    std::string colValName_;
    std::string colTypeName_;
    std::string colType_;
    std::string colDatabaseType_;
    std::string dbType_;
    ssize_t colLength_{0};
    size_t index_{0};
    bool isAutoVal_{false};
    bool isPrimaryKey_{false};
    bool notNull_{false};
    bool hasDefaultVal_{false};
};

inline std::string nameTransform(const std::string &origName, bool isType)
{
    auto str = origName;
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return tolower(c);
    });
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
            ++pos;
        if (str[pos] >= 'a' && str[pos] <= 'z')
            str[pos] += ('A' - 'a');
        startPos = pos;
    } while (1);
    if (isType && ret[0] >= 'a' && ret[0] <= 'z')
        ret[0] += ('A' - 'a');
    return ret;
}

std::string escapeIdentifier(const std::string &identifier,
                             const std::string &rdbms);

class PivotTable
{
  public:
    PivotTable() = default;

    PivotTable(const Json::Value &json)
        : tableName_(json.get("table_name", "").asString())
    {
        if (tableName_.empty())
        {
            throw std::runtime_error("table_name can't be empty");
        }
        originalKey_ = json.get("original_key", "").asString();
        if (originalKey_.empty())
        {
            throw std::runtime_error("original_key can't be empty");
        }
        targetKey_ = json.get("target_key", "").asString();
        if (targetKey_.empty())
        {
            throw std::runtime_error("target_key can't be empty");
        }
    }

    PivotTable reverse() const
    {
        PivotTable pivot;
        pivot.tableName_ = tableName_;
        pivot.originalKey_ = targetKey_;
        pivot.targetKey_ = originalKey_;
        return pivot;
    }

    const std::string &tableName() const
    {
        return tableName_;
    }

    const std::string &originalKey() const
    {
        return originalKey_;
    }

    const std::string &targetKey() const
    {
        return targetKey_;
    }

  private:
    std::string tableName_;
    std::string originalKey_;
    std::string targetKey_;
};

class ConvertMethod
{
  public:
    ConvertMethod(const Json::Value &convert)
    {
        tableName_ = convert.get("table", "*").asString();
        colName_ = convert.get("column", "*").asString();

        auto method = convert["method"];
        if (method.isNull())
        {
            throw std::runtime_error("method - object is missing.");
        }  // endif
        if (!method.isObject())
        {
            throw std::runtime_error("method is not an object.");
        }  // endif
        methodBeforeDbWrite_ = method.get("before_db_write", "").asString();
        methodAfterDbRead_ = method.get("after_db_read", "").asString();

        auto includeFiles = convert["includes"];
        if (includeFiles.isNull())
        {
            return;
        }  // endif
        if (!includeFiles.isArray())
        {
            throw std::runtime_error("includes must be an array");
        }  // endif
        for (auto &i : includeFiles)
        {
            includeFiles_.push_back(i.asString());
        }  // for
    }

    ConvertMethod() = default;

    bool shouldConvert(const std::string &tableName,
                       const std::string &colName) const;

    const std::string &tableName() const
    {
        return tableName_;
    }

    const std::string &colName() const
    {
        return colName_;
    }

    const std::string &methodBeforeDbWrite() const
    {
        return methodBeforeDbWrite_;
    }

    const std::string &methodAfterDbRead() const
    {
        return methodAfterDbRead_;
    }

    const std::vector<std::string> &includeFiles() const
    {
        return includeFiles_;
    }

  private:
    std::string tableName_{"*"};
    std::string colName_{"*"};
    std::string methodBeforeDbWrite_;
    std::string methodAfterDbRead_;
    std::vector<std::string> includeFiles_;
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
            type_ = Relationship::Type::HasOne;
        }
        else if (type == "has many")
        {
            type_ = Relationship::Type::HasMany;
        }
        else if (type == "many to many")
        {
            type_ = Relationship::Type::ManyToMany;
        }
        else
        {
            char message[128];
            snprintf(message,
                     sizeof(message),
                     "Invalid relationship type: %s",
                     type.data());
            throw std::runtime_error(message);
        }
        originalTableName_ =
            relationship.get("original_table_name", "").asString();
        if (originalTableName_.empty())
        {
            throw std::runtime_error("original_table_name can't be empty");
        }
        originalKey_ = relationship.get("original_key", "").asString();
        if (originalKey_.empty())
        {
            throw std::runtime_error("original_key can't be empty");
        }
        originalTableAlias_ =
            relationship.get("original_table_alias", "").asString();
        targetTableName_ = relationship.get("target_table_name", "").asString();
        if (targetTableName_.empty())
        {
            throw std::runtime_error("target_table_name can't be empty");
        }
        targetKey_ = relationship.get("target_key", "").asString();
        if (targetKey_.empty())
        {
            throw std::runtime_error("target_key can't be empty");
        }
        targetTableAlias_ =
            relationship.get("target_table_alias", "").asString();
        enableReverse_ = relationship.get("enable_reverse", false).asBool();
        if (type_ == Type::ManyToMany)
        {
            auto &pivot = relationship["pivot_table"];
            if (pivot.isNull())
            {
                throw std::runtime_error(
                    "ManyToMany relationship needs a pivot table");
            }
            pivotTable_ = PivotTable(pivot);
        }
    }

    Relationship() = default;

    Relationship reverse() const
    {
        Relationship r;
        if (type_ == Type::HasMany)
        {
            r.type_ = Type::HasOne;
        }
        else
        {
            r.type_ = type_;
        }
        r.originalTableName_ = targetTableName_;
        r.originalTableAlias_ = targetTableAlias_;
        r.originalKey_ = targetKey_;
        r.targetTableName_ = originalTableName_;
        r.targetTableAlias_ = originalTableAlias_;
        r.targetKey_ = originalKey_;
        r.enableReverse_ = enableReverse_;
        r.pivotTable_ = pivotTable_.reverse();
        return r;
    }

    Type type() const
    {
        return type_;
    }

    bool enableReverse() const
    {
        return enableReverse_;
    }

    const std::string &originalTableName() const
    {
        return originalTableName_;
    }

    const std::string &originalTableAlias() const
    {
        return originalTableAlias_;
    }

    const std::string &originalKey() const
    {
        return originalKey_;
    }

    const std::string &targetTableName() const
    {
        return targetTableName_;
    }

    const std::string &targetTableAlias() const
    {
        return targetTableAlias_;
    }

    const std::string &targetKey() const
    {
        return targetKey_;
    }

    const PivotTable &pivotTable() const
    {
        return pivotTable_;
    }

  private:
    Type type_{Type::HasOne};
    std::string originalTableName_;
    std::string originalTableAlias_;
    std::string targetTableName_;
    std::string targetTableAlias_;
    std::string originalKey_;
    std::string targetKey_;
    bool enableReverse_{false};
    PivotTable pivotTable_;
};

class create_model : public DrObject<create_model>, public CommandHandler
{
  public:
    void handleCommand(std::vector<std::string> &parameters) override;

    std::string script() override
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
    void createModelClassFromPG(
        const std::string &path,
        const DbClientPtr &client,
        const std::string &tableName,
        const std::string &schema,
        const Json::Value &restfulApiConfig,
        const std::vector<Relationship> &relationships,
        const std::vector<ConvertMethod> &convertMethods);

    void createModelFromPG(
        const std::string &path,
        const DbClientPtr &client,
        const std::string &schema,
        const Json::Value &restfulApiConfig,
        std::map<std::string, std::vector<Relationship>> &relationships,
        std::map<std::string, std::vector<ConvertMethod>> &convertMethods);
#endif
#if USE_MYSQL
    void createModelClassFromMysql(
        const std::string &path,
        const DbClientPtr &client,
        const std::string &tableName,
        const Json::Value &restfulApiConfig,
        const std::vector<Relationship> &relationships,
        const std::vector<ConvertMethod> &convertMethods);
    void createModelFromMysql(
        const std::string &path,
        const DbClientPtr &client,
        const Json::Value &restfulApiConfig,
        std::map<std::string, std::vector<Relationship>> &relationships,
        std::map<std::string, std::vector<ConvertMethod>> &convertMethods);
#endif
#if USE_SQLITE3
    void createModelClassFromSqlite3(
        const std::string &path,
        const DbClientPtr &client,
        const std::string &tableName,
        const Json::Value &restfulApiConfig,
        const std::vector<Relationship> &relationships,
        const std::vector<ConvertMethod> &convertMethod);
    void createModelFromSqlite3(
        const std::string &path,
        const DbClientPtr &client,
        const Json::Value &restfulApiConfig,
        std::map<std::string, std::vector<Relationship>> &relationships,
        std::map<std::string, std::vector<ConvertMethod>> &convertMethod);
#endif
    void createRestfulAPIController(const DrTemplateData &tableInfo,
                                    const Json::Value &restfulApiConfig);
    std::string dbname_;
    bool forceOverwrite_{false};
    std::string outputPath_;
};
}  // namespace drogon_ctl
