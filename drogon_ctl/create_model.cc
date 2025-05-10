/**
 *
 *  create_model.cc
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

#include "create_model.h"
#include "cmd.h"
#include <drogon/config.h>
#include <drogon/utils/Utilities.h>
#include <drogon/HttpViewData.h>
#include <drogon/DrTemplateBase.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <algorithm>
#ifndef _WIN32
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#else
#include <io.h>
#endif
#include <thread>
#include <chrono>

using namespace std::chrono_literals;
using namespace drogon_ctl;

static std::string toLower(const std::string &str)
{
    auto ret = str;
    std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) {
        return tolower(c);
    });
    return ret;
}

static std::string escapeConnString(const std::string &str)
{
    bool beQuoted = str.empty() || (str.find(' ') != std::string::npos);

    std::string escaped;
    escaped.reserve(str.size());
    for (auto ch : str)
    {
        if (ch == '\'')
            escaped.push_back('\\');
        else if (ch == '\\')
            escaped.push_back('\\');
        escaped.push_back(ch);
    }

    if (beQuoted)
        return "'" + escaped + "'";
    return escaped;
}

std::string drogon_ctl::escapeIdentifier(const std::string &identifier,
                                         const std::string &rdbms)
{
    if (rdbms != "postgresql")
    {
        return identifier;
    }

    return "\\\"" + identifier + "\\\"";
}

static std::map<std::string, std::vector<ConvertMethod>> getConvertMethods(
    const Json::Value &convertColumns)
{
    std::map<std::string, std::vector<ConvertMethod>> ret;
    auto enabled = convertColumns.get("enabled", false).asBool();
    if (!enabled)
    {
        return ret;
    }  // endif
    auto items = convertColumns["items"];
    if (items.isNull())
    {
        return ret;
    }  // endif
    if (!items.isArray())
    {
        std::cerr << "items of convert must be an array" << std::endl;
        exit(1);
    }  // endif
    for (auto &convertColumn : items)
    {
        try
        {
            ConvertMethod c(convertColumn);
            ret[c.tableName()].push_back(c);
        }  // try
        catch (const std::runtime_error &e)
        {
            std::cerr << e.what() << std::endl;
            exit(1);
        }  // catch
    }      // for

    return ret;
}

static std::map<std::string, std::vector<Relationship>> getRelationships(
    const Json::Value &relationships)
{
    std::map<std::string, std::vector<Relationship>> ret;
    auto enabled = relationships.get("enabled", false).asBool();
    if (!enabled)
        return ret;
    auto items = relationships["items"];
    if (items.isNull())
        return ret;
    if (!items.isArray())
    {
        std::cerr << "items must be an array\n";
        exit(1);
    }
    for (auto &relationship : items)
    {
        try
        {
            Relationship r(relationship);
            ret[r.originalTableName()].push_back(r);
            if (r.enableReverse() &&
                (r.originalTableName() != r.targetTableName() ||
                 r.originalTableAlias() != r.targetTableAlias()))
            {
                auto reverse = r.reverse();
                ret[reverse.originalTableName()].push_back(reverse);
            }
        }
        catch (const std::runtime_error &e)
        {
            std::cerr << e.what() << std::endl;
            exit(1);
        }
    }
    return ret;
}

bool drogon_ctl::ConvertMethod::shouldConvert(const std::string &tableName,
                                              const std::string &colName) const
{
    if (tableName == "*")
    {
        return colName == colName_;
    }
    else
    {
        return (tableName == tableName_ && colName == colName_);
    }  // endif
}

#if USE_POSTGRESQL
void create_model::createModelClassFromPG(
    const std::string &path,
    const DbClientPtr &client,
    const std::string &tableName,
    const std::string &schema,
    const Json::Value &restfulApiConfig,
    const std::vector<Relationship> &relationships,
    const std::vector<ConvertMethod> &convertMethods)
{
    auto className = nameTransform(tableName, true);
    HttpViewData data;
    data["className"] = className;
    data["tableName"] = tableName;
    data["hasPrimaryKey"] = (int)0;
    data["primaryKeyName"] = "";
    data["dbName"] = dbname_;
    data["rdbms"] = std::string("postgresql");
    data["relationships"] = relationships;
    data["convertMethods"] = convertMethods;
    if (schema != "public")
    {
        data["schema"] = schema;
    }
    std::vector<ColumnInfo> cols;
    *client << "SELECT * "
               "FROM information_schema.columns "
               "WHERE table_schema = $1 "
               "AND table_name   = $2"
            << schema << tableName << Mode::Blocking >>
        [&](const Result &r) {
            if (r.size() == 0)
            {
                std::cout << "    ---Can't create model from the table "
                          << tableName
                          << ", please check privileges on the table."
                          << std::endl;
                return;
            }
            for (Result::SizeType i = 0; i < r.size(); ++i)
            {
                auto row = r[i];
                ColumnInfo info;
                info.index_ = i;
                info.dbType_ = "pg";
                info.colName_ = row["column_name"].as<std::string>();
                info.colTypeName_ = nameTransform(info.colName_, true);
                info.colValName_ = nameTransform(info.colName_, false);
                auto isNullAble = row["is_nullable"].as<std::string>();
                info.notNull_ = isNullAble == "YES" ? false : true;
                auto type = row["data_type"].as<std::string>();
                info.colDatabaseType_ = type;
                if (type == "smallint")
                {
                    info.colType_ = "short";
                    info.colLength_ = 2;
                }
                else if (type == "integer")
                {
                    info.colType_ = "int32_t";
                    info.colLength_ = 4;
                }
                else if (type == "bigint")
                {
                    info.colType_ = "int64_t";
                    info.colLength_ = 8;
                }
                else if (type == "real")
                {
                    info.colType_ = "float";
                    info.colLength_ = sizeof(float);
                }
                else if (type == "double precision")
                {
                    info.colType_ = "double";
                    info.colLength_ = sizeof(double);
                }
                else if (type == "character varying")
                {
                    info.colType_ = "std::string";
                    if (!row["character_maximum_length"].isNull())
                        info.colLength_ =
                            row["character_maximum_length"].as<ssize_t>();
                }
                else if (type == "boolean")
                {
                    info.colType_ = "bool";
                    info.colLength_ = 1;
                }
                else if (type == "date")
                {
                    info.colType_ = "::trantor::Date";
                }
                else if (type.find("timestamp") != std::string::npos)
                {
                    info.colType_ = "::trantor::Date";
                }
                else if (type == "bytea")
                {
                    info.colType_ = "std::vector<char>";
                }
                else if (type.find("numeric") != std::string::npos)
                {
                    info.colType_ = "std::string";
                }
                else
                {
                    info.colType_ = "std::string";
                }
                auto defaultVal = row["column_default"].as<std::string>();
                if (!defaultVal.empty())
                {
                    info.hasDefaultVal_ = true;
                    if (defaultVal.find("nextval(") == 0)
                    {
                        info.isAutoVal_ = true;
                    }
                }
                auto isIdentity = row["is_identity"].as<std::string>();
                if (isIdentity == "YES")
                {
                    info.isAutoVal_ = true;
                }
                cols.push_back(std::move(info));
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
    size_t pkNumber = 0;
    *client << "SELECT "
               "pg_constraint.conname AS pk_name,"
               "pg_constraint.conkey AS pk_vector "
               "FROM pg_constraint "
               "INNER JOIN pg_class ON pg_constraint.conrelid = pg_class.oid "
               "WHERE "
               "pg_class.relname = $1 "
               "AND pg_constraint.contype = 'p'"
            << tableName << Mode::Blocking >>
        [&](bool isNull,
            const std::string &pkName,
            const std::vector<std::shared_ptr<short>> &pk) {
            if (!isNull)
            {
                pkNumber = pk.size();
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
    data["hasPrimaryKey"] = (int)pkNumber;
    if (pkNumber == 1)
    {
        *client << "SELECT "
                   "pg_attribute.attname AS colname,"
                   "pg_type.typname AS typename,"
                   "pg_constraint.contype AS contype "
                   "FROM pg_constraint "
                   "INNER JOIN pg_class ON pg_constraint.conrelid = "
                   "pg_class.oid "
                   "INNER JOIN pg_attribute ON pg_attribute.attrelid = "
                   "pg_class.oid "
                   "AND pg_attribute.attnum = pg_constraint.conkey [ 1 ] "
                   "INNER JOIN pg_type ON pg_type.oid = pg_attribute.atttypid "
                   "WHERE pg_class.relname = $1 and pg_constraint.contype='p'"
                << tableName << Mode::Blocking >>
            [&](bool isNull,
                const std::string &colName,
                const std::string &type) {
                if (isNull)
                    return;

                data["primaryKeyName"] = colName;
                for (auto &col : cols)
                {
                    if (col.colName_ == colName)
                    {
                        col.isPrimaryKey_ = true;
                        data["primaryKeyType"] = col.colType_;
                    }
                }
            } >>
            [](const DrogonDbException &e) {
                std::cerr << e.base().what() << std::endl;
                exit(1);
            };
    }
    else if (pkNumber > 1)
    {
        std::vector<std::string> pkNames, pkTypes, pkValNames;
        for (size_t i = 1; i <= pkNumber; ++i)
        {
            *client << "SELECT "
                       "pg_attribute.attname AS colname,"
                       "pg_type.typname AS typename,"
                       "pg_constraint.contype AS contype "
                       "FROM pg_constraint "
                       "INNER JOIN pg_class ON pg_constraint.conrelid = "
                       "pg_class.oid "
                       "INNER JOIN pg_attribute ON pg_attribute.attrelid = "
                       "pg_class.oid "
                       "AND pg_attribute.attnum = pg_constraint.conkey [ $1 ] "
                       "INNER JOIN pg_type ON pg_type.oid = "
                       "pg_attribute.atttypid "
                       "WHERE pg_class.relname = $2 and "
                       "pg_constraint.contype='p'"
                    << (int)i << tableName << Mode::Blocking >>
                [&](bool isNull, std::string colName, const std::string &type) {
                    if (isNull)
                        return;
                    pkNames.push_back(colName);
                    pkValNames.push_back(nameTransform(colName, false));
                    for (auto &col : cols)
                    {
                        if (col.colName_ == colName)
                        {
                            col.isPrimaryKey_ = true;
                            pkTypes.push_back(col.colType_);
                        }
                    }
                } >>
                [](const DrogonDbException &e) {
                    std::cerr << e.base().what() << std::endl;
                    exit(1);
                };
        }
        data["primaryKeyName"] = pkNames;
        data["primaryKeyType"] = pkTypes;
        data["primaryKeyValNames"] = pkValNames;
    }

    data["columns"] = cols;
    std::ofstream headerFile(path + "/" + className + ".h", std::ofstream::out);
    std::ofstream sourceFile(path + "/" + className + ".cc",
                             std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("model_h.csp");
    headerFile << templ->genText(data);
    templ = DrTemplateBase::newTemplate("model_cc.csp");
    sourceFile << templ->genText(data);
    createRestfulAPIController(data, restfulApiConfig);
}

void create_model::createModelFromPG(
    const std::string &path,
    const DbClientPtr &client,
    const std::string &schema,
    const Json::Value &restfulApiConfig,
    std::map<std::string, std::vector<Relationship>> &relationships,
    std::map<std::string, std::vector<ConvertMethod>> &convertMethods)
{
    *client << "SELECT a.oid,"
               "a.relname AS name,"
               "b.description AS comment "
               "FROM pg_class a "
               "LEFT OUTER JOIN pg_description b ON b.objsubid = 0 AND a.oid = "
               "b.objoid "
               "WHERE a.relnamespace = (SELECT oid FROM pg_namespace WHERE "
               "nspname = $1) "
               "AND a.relkind = 'r' ORDER BY a.relname"
            << schema << Mode::Blocking >>
        [&](bool isNull,
            size_t oid,
            std::string &&tableName,
            const std::string &comment) {
            if (!isNull)
            {
                std::cout << "table name:" << tableName << std::endl;

                createModelClassFromPG(path,
                                       client,
                                       tableName,
                                       schema,
                                       restfulApiConfig,
                                       relationships[tableName],
                                       convertMethods[tableName]);
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
}
#endif

#if USE_MYSQL
void create_model::createModelClassFromMysql(
    const std::string &path,
    const DbClientPtr &client,
    const std::string &tableName,
    const Json::Value &restfulApiConfig,
    const std::vector<Relationship> &relationships,
    const std::vector<ConvertMethod> &convertMethods)
{
    auto className = nameTransform(tableName, true);
    HttpViewData data;
    data["className"] = className;
    data["tableName"] = toLower(tableName);
    data["hasPrimaryKey"] = (int)0;
    data["primaryKeyName"] = "";
    data["dbName"] = dbname_;
    data["rdbms"] = std::string("mysql");
    data["relationships"] = relationships;
    data["convertMethods"] = convertMethods;
    std::vector<ColumnInfo> cols;
    int i = 0;
    *client << "desc `" + tableName + "`" << Mode::Blocking >>
        [&i, &cols](bool isNull,
                    const std::string &field,
                    const std::string &type,
                    const std::string &isNullAble,
                    const std::string &key,
                    const std::string &defaultVal,
                    const std::string &extra) {
            if (!isNull)
            {
                ColumnInfo info;
                info.index_ = i;
                info.dbType_ = "mysql";
                info.colName_ = field;
                info.colTypeName_ = nameTransform(info.colName_, true);
                info.colValName_ = nameTransform(info.colName_, false);
                info.notNull_ = isNullAble == "YES" ? false : true;
                info.colDatabaseType_ = type;
                info.isPrimaryKey_ = key == "PRI" ? true : false;
                if (type.find("tinyint") == 0)
                {
                    info.colType_ = "int8_t";
                    info.colLength_ = 1;
                }
                else if (type.find("smallint") == 0)
                {
                    info.colType_ = "int16_t";
                    info.colLength_ = 2;
                }
                else if (type.find("mediumint") == 0)
                {
                    info.colType_ = "int32_t";
                    info.colLength_ = 3;
                }
                else if (type.find("int") == 0)
                {
                    info.colType_ = "int32_t";
                    info.colLength_ = 4;
                }
                else if (type.find("bigint") == 0)
                {
                    info.colType_ = "int64_t";
                    info.colLength_ = 8;
                }
                else if (type.find("float") == 0)
                {
                    info.colType_ = "float";
                    info.colLength_ = sizeof(float);
                }
                else if (type.find("double") == 0)
                {
                    info.colType_ = "double";
                    info.colLength_ = sizeof(double);
                }
                else if (type.find("date") == 0 || type.find("datetime") == 0 ||
                         type.find("timestamp") == 0)
                {
                    info.colType_ = "::trantor::Date";
                }
                else if (type.find("blob") != std::string::npos)
                {
                    info.colType_ = "std::vector<char>";
                }
                else if (type.find("varchar") != std::string::npos)
                {
                    info.colType_ = "std::string";
                    auto pos1 = type.find("(");
                    auto pos2 = type.find(")");
                    if (pos1 != std::string::npos &&
                        pos2 != std::string::npos && pos2 - pos1 > 1)
                    {
                        info.colLength_ =
                            std::stoll(type.substr(pos1 + 1, pos2 - pos1 - 1));
                    }
                }
                else
                {
                    info.colType_ = "std::string";
                }
                if (type.find("unsigned") != std::string::npos &&
                    info.colType_ != "std::string")
                {
                    info.colType_ = "u" + info.colType_;
                }
                if (!defaultVal.empty())
                {
                    info.hasDefaultVal_ = true;
                }
                if (extra.find("auto_") == 0)
                {
                    info.isAutoVal_ = true;
                }
                cols.push_back(std::move(info));
                ++i;
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
    std::vector<std::string> pkNames, pkTypes, pkValNames;
    for (auto const &col : cols)
    {
        if (col.isPrimaryKey_)
        {
            pkNames.push_back(col.colName_);
            pkValNames.push_back(nameTransform(col.colName_, false));
            pkTypes.push_back(col.colType_);
        }
    }
    data["hasPrimaryKey"] = (int)pkNames.size();
    if (pkNames.size() == 1)
    {
        data["primaryKeyName"] = pkNames[0];
        data["primaryKeyType"] = pkTypes[0];
    }
    else if (pkNames.size() > 1)
    {
        data["primaryKeyName"] = pkNames;
        data["primaryKeyType"] = pkTypes;
        data["primaryKeyValNames"] = pkValNames;
    }
    data["columns"] = cols;
    std::ofstream headerFile(path + "/" + className + ".h", std::ofstream::out);
    std::ofstream sourceFile(path + "/" + className + ".cc",
                             std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("model_h.csp");
    headerFile << templ->genText(data);
    templ = DrTemplateBase::newTemplate("model_cc.csp");
    sourceFile << templ->genText(data);
    createRestfulAPIController(data, restfulApiConfig);
}

void create_model::createModelFromMysql(
    const std::string &path,
    const DbClientPtr &client,
    const Json::Value &restfulApiConfig,
    std::map<std::string, std::vector<Relationship>> &relationships,
    std::map<std::string, std::vector<ConvertMethod>> &convertMethods)
{
    *client << "show tables" << Mode::Blocking >> [&](bool isNull,
                                                      std::string &&tableName) {
        if (!isNull)
        {
            std::cout << "table name:" << tableName << std::endl;
            createModelClassFromMysql(path,
                                      client,
                                      tableName,
                                      restfulApiConfig,
                                      relationships[tableName],
                                      convertMethods[tableName]);
        }
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
        exit(1);
    };
}
#endif
#if USE_SQLITE3
void create_model::createModelClassFromSqlite3(
    const std::string &path,
    const DbClientPtr &client,
    const std::string &tableName,
    const Json::Value &restfulApiConfig,
    const std::vector<Relationship> &relationships,
    const std::vector<ConvertMethod> &convertMethods)
{
    HttpViewData data;
    auto className = nameTransform(tableName, true);
    data["className"] = className;
    data["tableName"] = toLower(tableName);
    data["hasPrimaryKey"] = (int)0;
    data["primaryKeyName"] = "";
    data["dbName"] = std::string("sqlite3");
    data["rdbms"] = std::string("sqlite3");
    data["relationships"] = relationships;
    data["convertMethods"] = convertMethods;
    std::vector<ColumnInfo> cols;
    std::string sql = "PRAGMA table_info(" + tableName + ");";
    *client << sql << Mode::Blocking >> [&](const Result &result) {
        size_t index = 0;
        for (auto &row : result)
        {
            bool notnull = row["notnull"].as<bool>();
            bool primary = row["pk"].as<int>();
            auto type = row["type"].as<std::string>();
            std::transform(type.begin(),
                           type.end(),
                           type.begin(),
                           [](unsigned char c) { return tolower(c); });
            ColumnInfo info;
            info.index_ = index++;
            info.dbType_ = "sqlite3";
            info.colName_ = row["name"].as<std::string>();
            info.colTypeName_ = nameTransform(info.colName_, true);
            info.colValName_ = nameTransform(info.colName_, false);
            info.notNull_ = notnull;
            info.colDatabaseType_ = type;
            info.isPrimaryKey_ = primary;
            if (primary)
            {
                if (type == "integer")
                {
                    info.isAutoVal_ = true;
                }
                else
                {
                    *client << "SELECT sql FROM sqlite_master WHERE name=? and "
                               "(type='table' or type='view');"
                            << tableName << Mode::Blocking >>
                        [&](bool isNull, std::string sql) {
                            if (!isNull)
                            {
                                std::transform(sql.begin(),
                                               sql.end(),
                                               sql.begin(),
                                               [](unsigned char c) {
                                                   return tolower(c);
                                               });
                                if (sql.find("autoincrement") !=
                                    std::string::npos)
                                {
                                    info.isAutoVal_ = true;
                                }
                            }
                        } >>
                        [](const DrogonDbException &e) {

                        };
                }
            }
            auto defaultVal = row["dflt_value"].as<std::string>();
            if (!defaultVal.empty())
            {
                info.hasDefaultVal_ = true;
            }

            if (type.find("int") != std::string::npos)
            {
                info.colType_ = "int64_t";
                info.colLength_ = 8;
            }
            else if (type.find("char") != std::string::npos || type == "text" ||
                     type == "clob")
            {
                info.colType_ = "std::string";
            }
            else if (type.find("double") != std::string::npos ||
                     type == "real" || type == "float")
            {
                info.colType_ = "double";
                info.colLength_ = sizeof(double);
            }
            else if (type == "bool")
            {
                info.colType_ = "bool";
                info.colLength_ = 1;
            }
            else if (type == "blob")
            {
                info.colType_ = "std::vector<char>";
            }
            else if (type == "datetime" || type == "date")
            {
                info.colType_ = "::trantor::Date";
            }
            else
            {
                info.colType_ = "std::string";
            }
            cols.push_back(std::move(info));
        }
    } >> [](const DrogonDbException &e) {
        std::cerr << e.base().what() << std::endl;
        exit(1);
    };
    std::vector<std::string> pkNames, pkTypes, pkValNames;
    for (auto const &col : cols)
    {
        if (col.isPrimaryKey_)
        {
            pkNames.push_back(col.colName_);
            pkTypes.push_back(col.colType_);
            pkValNames.push_back(nameTransform(col.colName_, false));
        }
    }
    data["hasPrimaryKey"] = (int)pkNames.size();
    if (pkNames.size() == 1)
    {
        data["primaryKeyName"] = pkNames[0];
        data["primaryKeyType"] = pkTypes[0];
    }
    else if (pkNames.size() > 1)
    {
        for (auto &col : cols)
        {
            col.isAutoVal_ = false;
        }

        data["primaryKeyName"] = pkNames;
        data["primaryKeyType"] = pkTypes;
        data["primaryKeyValNames"] = pkValNames;
    }
    data["columns"] = cols;
    std::ofstream headerFile(path + "/" + className + ".h", std::ofstream::out);
    std::ofstream sourceFile(path + "/" + className + ".cc",
                             std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("model_h.csp");
    headerFile << templ->genText(data);
    templ = DrTemplateBase::newTemplate("model_cc.csp");
    sourceFile << templ->genText(data);
    createRestfulAPIController(data, restfulApiConfig);
}

void create_model::createModelFromSqlite3(
    const std::string &path,
    const DbClientPtr &client,
    const Json::Value &restfulApiConfig,
    std::map<std::string, std::vector<Relationship>> &relationships,
    std::map<std::string, std::vector<ConvertMethod>> &convertMethods)
{
    *client << "SELECT name FROM sqlite_master WHERE name!='sqlite_sequence' "
               "and (type='table' or type='view') ORDER BY name;"
            << Mode::Blocking >>
        [&](bool isNull, std::string &&tableName) mutable {
            if (!isNull)
            {
                std::cout << "table name:" << tableName << std::endl;
                createModelClassFromSqlite3(path,
                                            client,
                                            tableName,
                                            restfulApiConfig,
                                            relationships[tableName],
                                            convertMethods[tableName]);
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
}
#endif

void create_model::createModel(const std::string &path,
                               const Json::Value &config,
                               const std::string &singleModelName)
{
    auto dbType = config.get("rdbms", "no dbms").asString();
    std::transform(dbType.begin(),
                   dbType.end(),
                   dbType.begin(),
                   [](unsigned char c) { return tolower(c); });
    auto restfulApiConfig = config["restful_api_controllers"];
    auto relationships = getRelationships(config["relationships"]);
    auto convertMethods = getConvertMethods(config["convert"]);
    drogon::utils::createPath(path);
    if (dbType == "postgresql")
    {
#if USE_POSTGRESQL
        std::cout << "postgresql" << std::endl;
        auto host = config.get("host", "127.0.0.1").asString();
        auto port = config.get("port", 5432).asUInt();
        auto dbname = config.get("dbname", "").asString();
        if (dbname == "")
        {
            std::cerr << "Please configure dbname in " << path << "/model.json "
                      << std::endl;
            exit(1);
        }
        dbname_ = dbname;
        auto user = config.get("user", "").asString();
        if (user == "")
        {
            std::cerr << "Please configure user in " << path << "/model.json "
                      << std::endl;
            exit(1);
        }
        auto password = config.get("passwd", "").asString();
        if (password.empty())
        {
            password = config.get("password", "").asString();
        }

        auto connStr =
            utils::formattedString("host=%s port=%u dbname=%s user=%s",
                                   escapeConnString(host).c_str(),
                                   port,
                                   escapeConnString(dbname).c_str(),
                                   escapeConnString(user).c_str());
        if (!password.empty())
        {
            connStr += " password=";
            connStr += escapeConnString(password);
        }
        auto characterSet = config.get("client_encoding", "").asString();
        if (!characterSet.empty())
        {
            connStr += " client_encoding=";
            connStr += escapeConnString(characterSet);
        }

        auto schema = config.get("schema", "public").asString();
        DbClientPtr client = drogon::orm::DbClient::newPgClient(connStr, 1);
        std::cout << "Connect to server..." << std::endl;
        if (forceOverwrite_)
        {
            std::this_thread::sleep_for(2s);
        }
        else
        {
            std::cout << "Source files in the " << path
                      << " folder will be overwritten, continue(y/n)?\n";
            auto in = getchar();
            (void)getchar();  // get the return key
            if (in != 'Y' && in != 'y')
            {
                std::cout << "Abort!" << std::endl;
                exit(0);
            }
        }

        if (singleModelName.empty())
        {
            auto tables = config["tables"];
            if (!tables || tables.size() == 0)
                createModelFromPG(path,
                                  client,
                                  schema,
                                  restfulApiConfig,
                                  relationships,
                                  convertMethods);
            else
            {
                for (int i = 0; i < (int)tables.size(); ++i)
                {
                    auto tableName = tables[i].asString();
                    std::transform(tableName.begin(),
                                   tableName.end(),
                                   tableName.begin(),
                                   [](unsigned char c) { return tolower(c); });
                    std::cout << "table name:" << tableName << std::endl;
                    createModelClassFromPG(path,
                                           client,
                                           tableName,
                                           schema,
                                           restfulApiConfig,
                                           relationships[tableName],
                                           convertMethods[tableName]);
                }
            }
        }
        else
        {
            createModelClassFromPG(path,
                                   client,
                                   singleModelName,
                                   schema,
                                   restfulApiConfig,
                                   relationships[singleModelName],
                                   convertMethods[singleModelName]);
        }
#else
        std::cerr
            << "Drogon does not support PostgreSQL, please install PostgreSQL "
               "development environment before installing drogon"
            << std::endl;
#endif
    }
    else if (dbType == "mysql")
    {
#if USE_MYSQL
        std::cout << "mysql" << std::endl;
        auto host = config.get("host", "127.0.0.1").asString();
        auto port = config.get("port", 5432).asUInt();
        auto dbname = config.get("dbname", "").asString();
        if (dbname == "")
        {
            std::cerr << "Please configure dbname in " << path << "/model.json "
                      << std::endl;
            exit(1);
        }
        dbname_ = dbname;
        auto user = config.get("user", "").asString();
        if (user == "")
        {
            std::cerr << "Please configure user in " << path << "/model.json "
                      << std::endl;
            exit(1);
        }
        auto password = config.get("passwd", "").asString();
        if (password.empty())
        {
            password = config.get("password", "").asString();
        }

        auto connStr =
            utils::formattedString("host=%s port=%u dbname=%s user=%s",
                                   escapeConnString(host).c_str(),
                                   port,
                                   escapeConnString(dbname).c_str(),
                                   escapeConnString(user).c_str());
        if (!password.empty())
        {
            connStr += " password=";
            connStr += escapeConnString(password);
        }
        auto characterSet = config.get("client_encoding", "").asString();
        if (!characterSet.empty())
        {
            connStr += " client_encoding=";
            connStr += escapeConnString(characterSet);
        }
        DbClientPtr client = drogon::orm::DbClient::newMysqlClient(connStr, 1);
        std::cout << "Connect to server..." << std::endl;
        if (forceOverwrite_)
        {
            std::this_thread::sleep_for(2s);
        }
        else
        {
            std::cout << "Source files in the " << path
                      << " folder will be overwritten, continue(y/n)?\n";
            auto in = getchar();
            (void)getchar();  // get the return key
            if (in != 'Y' && in != 'y')
            {
                std::cout << "Abort!" << std::endl;
                exit(0);
            }
        }

        if (singleModelName.empty())
        {
            auto tables = config["tables"];
            if (!tables || tables.size() == 0)
                createModelFromMysql(path,
                                     client,
                                     restfulApiConfig,
                                     relationships,
                                     convertMethods);
            else
            {
                for (int i = 0; i < (int)tables.size(); ++i)
                {
                    auto tableName = tables[i].asString();
                    std::transform(tableName.begin(),
                                   tableName.end(),
                                   tableName.begin(),
                                   [](unsigned char c) { return tolower(c); });
                    std::cout << "table name:" << tableName << std::endl;
                    createModelClassFromMysql(path,
                                              client,
                                              tableName,
                                              restfulApiConfig,
                                              relationships[tableName],
                                              convertMethods[tableName]);
                }
            }
        }
        else
        {
            createModelClassFromMysql(path,
                                      client,
                                      singleModelName,
                                      restfulApiConfig,
                                      relationships[singleModelName],
                                      convertMethods[singleModelName]);
        }

#else
        std::cerr << "Drogon does not support Mysql, please install MariaDB "
                     "development environment before installing drogon"
                  << std::endl;
#endif
    }
    else if (dbType == "sqlite3")
    {
#if USE_SQLITE3
        auto filename = config.get("filename", "").asString();
        if (filename == "")
        {
            std::cerr << "Please configure filename in " << path
                      << "/model.json " << std::endl;
            exit(1);
        }
        std::string connStr = "filename=" + escapeConnString(filename);
        DbClientPtr client =
            drogon::orm::DbClient::newSqlite3Client(connStr, 1);
        std::cout << "Connect..." << std::endl;
        if (forceOverwrite_)
        {
            std::this_thread::sleep_for(1s);
        }
        else
        {
            std::cout << "Source files in the " << path
                      << " folder will be overwritten, continue(y/n)?\n";
            auto in = getchar();
            (void)getchar();  // get the return key
            if (in != 'Y' && in != 'y')
            {
                std::cout << "Abort!" << std::endl;
                exit(0);
            }
        }

        if (singleModelName.empty())
        {
            auto tables = config["tables"];
            if (!tables || tables.size() == 0)
                createModelFromSqlite3(path,
                                       client,
                                       restfulApiConfig,
                                       relationships,
                                       convertMethods);
            else
            {
                for (int i = 0; i < (int)tables.size(); ++i)
                {
                    auto tableName = tables[i].asString();
                    std::transform(tableName.begin(),
                                   tableName.end(),
                                   tableName.begin(),
                                   [](unsigned char c) { return tolower(c); });
                    std::cout << "table name:" << tableName << std::endl;
                    createModelClassFromSqlite3(path,
                                                client,
                                                tableName,
                                                restfulApiConfig,
                                                relationships[tableName],
                                                convertMethods[tableName]);
                }
            }
        }
        else
        {
            createModelClassFromSqlite3(path,
                                        client,
                                        singleModelName,
                                        restfulApiConfig,
                                        relationships[singleModelName],
                                        convertMethods[singleModelName]);
        }

#else
        std::cerr << "Drogon does not support Sqlite3, please install Sqlite3 "
                     "development environment before installing drogon"
                  << std::endl;
#endif
    }
    else if (dbType == "no dbms")
    {
        std::cerr << "Please configure Model in " << path << "/model.json "
                  << std::endl;
        exit(1);
    }
    else
    {
        std::cerr << "Does not support " << dbType << std::endl;
        exit(1);
    }
}

void create_model::createModel(const std::string &path,
                               const std::string &singleModelName)
{
#ifndef _WIN32
    DIR *dp;
    if ((dp = opendir(path.c_str())) == NULL)
    {
        std::cerr << "No such file or directory : " << path << std::endl;
        return;
    }
    closedir(dp);
#endif
    auto configFile = path + "/model.json";
#ifdef _WIN32
    if (_access(configFile.c_str(), 0) != 0)
#else
    if (access(configFile.c_str(), 0) != 0)
#endif
    {
        std::cerr << "Config file " << configFile << " not found!" << std::endl;
        exit(1);
    }
#ifdef _WIN32
    if (_access(configFile.c_str(), 04) != 0)
#else
    if (access(configFile.c_str(), R_OK) != 0)
#endif
    {
        std::cerr << "No permission to read config file " << configFile
                  << std::endl;
        exit(1);
    }

    std::ifstream infile(configFile.c_str(), std::ifstream::in);
    if (infile)
    {
        Json::Value configJsonRoot;
        try
        {
            infile >> configJsonRoot;
            createModel(outputPath_.empty() ? path : outputPath_,
                        configJsonRoot,
                        singleModelName);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Configuration file format error! in " << configFile
                      << ":" << std::endl;
            std::cerr << exception.what() << std::endl;
            exit(1);
        }
    }
}

void create_model::handleCommand(std::vector<std::string> &parameters)
{
    std::cout << "Create model" << std::endl;
    if (parameters.size() == 0)
    {
        std::cerr << "Missing Model path name!" << std::endl;
    }
    std::string singleModelName;
    for (auto iter = parameters.begin(); iter != parameters.end(); ++iter)
    {
        if ((*iter).find("--table=") == 0)
        {
            singleModelName = (*iter).substr(8);
            parameters.erase(iter);
            break;
        }
    }
    for (auto iter = parameters.begin(); iter != parameters.end(); ++iter)
    {
        if ((*iter) == "-f")
        {
            forceOverwrite_ = true;
            parameters.erase(iter);
            break;
        }
    }
    for (auto iter = parameters.begin(); iter != parameters.end(); ++iter)
    {
        auto &file = *iter;
        if (file == "-o" || file == "--output")
        {
            iter = parameters.erase(iter);
            if (iter != parameters.end())
            {
                outputPath_ = *iter;
                iter = parameters.erase(iter);
            }
            continue;
        }
    }

    for (auto const &path : parameters)
    {
        createModel(path, singleModelName);
    }
}

void create_model::createRestfulAPIController(
    const DrTemplateData &tableInfo,
    const Json::Value &restfulApiConfig)
{
    if (restfulApiConfig.isNull())
        return;
    if (!restfulApiConfig.get("enabled", false).asBool())
    {
        return;
    }
    auto genBaseOnly =
        restfulApiConfig.get("generate_base_only", false).asBool();
    auto modelClassName = tableInfo.get<std::string>("className");
    std::regex regex("\\*");
    auto resource = std::regex_replace(
        restfulApiConfig.get("resource_uri", "/*").asString(),
        regex,
        modelClassName);
    std::transform(resource.begin(),
                   resource.end(),
                   resource.begin(),
                   [](unsigned char c) { return tolower(c); });
    auto ctrlClassName =
        std::regex_replace(restfulApiConfig.get("class_name", "/*").asString(),
                           regex,
                           modelClassName);
    std::regex regex1("::");
    std::string ctlName =
        std::regex_replace(ctrlClassName, regex1, std::string("_"));
    auto v = utils::splitString(ctrlClassName, "::");

    drogon::DrTemplateData data;
    data.insert("className", v[v.size() - 1]);
    v.pop_back();
    data.insert("namespaceVector", v);
    data.insert("resource", resource);
    data.insert("fileName", ctlName);
    data.insert("tableName", tableInfo.get<std::string>("tableName"));
    data.insert("tableInfo", tableInfo);
    auto filters = restfulApiConfig["filters"];
    if (filters.isNull() || filters.empty() || !filters.isArray())
    {
        data.insert("filters", "");
    }
    else
    {
        std::string filtersStr;
        for (auto &filterName : filters)
        {
            filtersStr += ",\"";
            filtersStr.append(filterName.asString());
            filtersStr += '"';
        }
        data.insert("filters", filtersStr);
    }
    auto dbClientConfig = restfulApiConfig["db_client"];
    if (dbClientConfig.isNull() || dbClientConfig.empty())
    {
        data.insertAsString("dbClientName", "default");
        data.insert("isFastDbClient", false);
    }
    else
    {
        auto clientName = dbClientConfig.get("name", "default").asString();
        auto isFast = dbClientConfig.get("is_fast", false).asBool();
        data.insertAsString("dbClientName", clientName);
        data.insert("isFastDbClient", isFast);
    }
    auto dir = restfulApiConfig.get("directory", "controllers").asString();
    if (dir[dir.length() - 1] != '/')
    {
        dir += '/';
    }
    {
        std::string headFileName = dir + ctlName + "Base.h";
        std::string sourceFilename = dir + ctlName + "Base.cc";
        // {
        //     std::ifstream iHeadFile(headFileName.c_str(), std::ifstream::in);
        //     std::ifstream iSourceFile(sourceFilename.c_str(),
        //                               std::ifstream::in);

        //     if (iHeadFile || iSourceFile)
        //     {
        //         std::cout << "The " << headFileName << " and " <<
        //         sourceFilename
        //                   << " you want to create already exist, "
        //                      "overwrite it(y/n)?"
        //                   << std::endl;
        //         auto in = getchar();
        //         (void)getchar();  // get the return key
        //         if (in != 'Y' && in != 'y')
        //         {
        //             std::cout << "Abort!" << std::endl;
        //             exit(0);
        //         }
        //     }
        // }
        std::ofstream oHeadFile(headFileName.c_str(), std::ofstream::out);
        std::ofstream oSourceFile(sourceFilename.c_str(), std::ofstream::out);
        if (!oHeadFile || !oSourceFile)
        {
            perror("");
            exit(1);
        }
        try
        {
            auto templ =
                DrTemplateBase::newTemplate("restful_controller_base_h.csp");
            oHeadFile << templ->genText(data);
            templ =
                DrTemplateBase::newTemplate("restful_controller_base_cc.csp");
            oSourceFile << templ->genText(data);
        }
        catch (const std::exception &err)
        {
            std::cerr << err.what() << std::endl;
            exit(1);
        }
        std::cout << "create a http restful API controller base class:"
                  << ctrlClassName << "Base" << std::endl;
        std::cout << "file name: " << headFileName << ", " << sourceFilename
                  << std::endl
                  << std::endl;
    }
    if (!genBaseOnly)
    {
        std::string headFileName = dir + ctlName + ".h";
        std::string sourceFilename = dir + ctlName + ".cc";
        if (!forceOverwrite_)
        {
            std::ifstream iHeadFile(headFileName.c_str(), std::ifstream::in);
            std::ifstream iSourceFile(sourceFilename.c_str(),
                                      std::ifstream::in);

            if (iHeadFile || iSourceFile)
            {
                std::cout << "The " << headFileName << " and " << sourceFilename
                          << " you want to create already exist, "
                             "overwrite them(y/n)?"
                          << std::endl;
                auto in = getchar();
                (void)getchar();  // get the return key
                if (in != 'Y' && in != 'y')
                {
                    std::cout << "Abort!" << std::endl;
                    exit(0);
                }
            }
        }
        std::ofstream oHeadFile(headFileName.c_str(), std::ofstream::out);
        std::ofstream oSourceFile(sourceFilename.c_str(), std::ofstream::out);
        if (!oHeadFile || !oSourceFile)
        {
            perror("");
            exit(1);
        }
        try
        {
            auto templ =
                DrTemplateBase::newTemplate("restful_controller_custom_h.csp");
            oHeadFile << templ->genText(data);
            templ =
                DrTemplateBase::newTemplate("restful_controller_custom_cc.csp");
            oSourceFile << templ->genText(data);
        }
        catch (const std::exception &err)
        {
            std::cerr << err.what() << std::endl;
            exit(1);
        }

        std::cout << "create a http restful API controller class: "
                  << ctrlClassName << std::endl;
        std::cout << "file name: " << headFileName << ", " << sourceFilename
                  << std::endl
                  << std::endl;
    }
}
