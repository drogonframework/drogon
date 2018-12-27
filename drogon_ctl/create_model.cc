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
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fstream>
#include <unistd.h>

using namespace drogon_ctl;

std::string nameTransform(const std::string &origName, bool isType)
{
    auto str = origName;
    std::transform(str.begin(), str.end(), str.begin(), tolower);
    std::string::size_type startPos = 0;
    std::string::size_type pos;
    std::string ret;
    do
    {
        pos = str.find("_", startPos);
        if (pos != std::string::npos)
            ret += str.substr(startPos, pos - startPos);
        else
        {
            ret += str.substr(startPos);
            break;
        }
        while (str[pos] == '_')
            pos++;
        if (str[pos] >= 'a' && str[pos] <= 'z')
            str[pos] += ('A' - 'a');
        startPos = pos;
    } while (1);
    if (isType && ret[0] >= 'a' && ret[0] <= 'z')
        ret[0] += ('A' - 'a');
    return ret;
}

#if USE_POSTGRESQL
void create_model::createModelClassFromPG(const std::string &path, const DbClientPtr &client, const std::string &tableName)
{
    auto className = nameTransform(tableName, true);
    HttpViewData data;
    data["className"] = className;
    data["tableName"] = tableName;
    data["hasPrimaryKey"] = (int)0;
    data["primaryKeyName"] = "";
    data["dbName"] = _dbname;
    data["rdbms"] = std::string("postgresql");
    std::vector<ColumnInfo> cols;
    *client << "SELECT * \
                FROM information_schema.columns \
                WHERE table_schema = 'public' \
                AND table_name   = $1"
            << tableName << Mode::Blocking >>
        [&](const Result &r) {
            if (r.size() == 0)
            {
                std::cout << "    ---Can't create model from the table " << tableName << ", please check privileges on the table." << std::endl;
                return;
            }
            for (size_t i = 0; i < r.size(); i++)
            {
                auto row = r[i];
                ColumnInfo info;
                info._index = i;
                info._dbType = "pg";
                info._colName = row["column_name"].as<std::string>();
                info._colTypeName = nameTransform(info._colName, true);
                info._colValName = nameTransform(info._colName, false);
                auto isNullAble = row["is_nullable"].as<std::string>();

                info._notNull = isNullAble == "YES" ? false : true;
                auto type = row["data_type"].as<std::string>();
                info._colDatabaseType = type;
                if (type == "smallint")
                {
                    info._colType = "short";
                    info._colLength = 2;
                }
                else if (type == "integer")
                {
                    info._colType = "int32_t";
                    info._colLength = 4;
                }
                else if (type == "bigint" || type == "numeric") //FIXME:Use int64 to represent numeric type?
                {
                    info._colType = "int64_t";
                    info._colLength = 8;
                }
                else if (type == "real")
                {
                    info._colType = "float";
                    info._colLength = sizeof(float);
                }
                else if (type == "double precision")
                {
                    info._colType = "double";
                    info._colLength = sizeof(double);
                }
                else if (type == "character varying")
                {
                    info._colType = "std::string";
                    if (!row["character_maximum_length"].isNull())
                        info._colLength = row["character_maximum_length"].as<ssize_t>();
                }
                else if (type == "boolean")
                {
                    info._colType = "bool";
                    info._colLength = 1;
                }
                else if (type == "date")
                {
                    info._colType = "::trantor::Date";
                }
                else if (type.find("timestamp") != std::string::npos)
                {
                    info._colType = "::trantor::Date";
                }
                else if (type == "bytea")
                {
                    info._colType = "std::vector<char>";
                }
                else
                {
                    info._colType = "std::string";
                    //FIXME add more type such as hstore...
                }
                auto defaultVal = row["column_default"].as<std::string>();

                if (!defaultVal.empty())
                {
                    info._hasDefaultVal = true;
                    if (defaultVal.find("nextval(") == 0)
                    {
                        info._isAutoVal = true;
                    }
                }
                cols.push_back(std::move(info));
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };

    size_t pkNumber = 0;
    *client << "SELECT \
                pg_constraint.conname AS pk_name,\
                pg_constraint.conkey AS pk_vector \
                FROM pg_constraint \
                INNER JOIN pg_class ON pg_constraint.conrelid = pg_class.oid \
                WHERE \
                pg_class.relname = $1 \
                AND pg_constraint.contype = 'p'"
            << tableName
            << Mode::Blocking >>
        [&](bool isNull, const std::string &pkName, const std::vector<std::shared_ptr<short>> &pk) {
            if (!isNull)
            {
                //std::cout << tableName << " Primary key = " << pk.size() << std::endl;
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
        *client << "SELECT \
                pg_attribute.attname AS colname,\
                pg_type.typname AS typename,\
                pg_constraint.contype AS contype \
                FROM pg_constraint \
                INNER JOIN pg_class ON pg_constraint.conrelid = pg_class.oid \
                INNER JOIN pg_attribute ON pg_attribute.attrelid = pg_class.oid \
                AND pg_attribute.attnum = pg_constraint.conkey [ 1 ] \
                INNER JOIN pg_type ON pg_type.oid = pg_attribute.atttypid \
                WHERE pg_class.relname = $1 and pg_constraint.contype='p'"
                << tableName << Mode::Blocking >>
            [&](bool isNull, std::string colName, const std::string &type) {
                if (isNull)
                    return;

                data["primaryKeyName"] = colName;
                for (auto &col : cols)
                {
                    if (col._colName == colName)
                    {
                        col._isPrimaryKey = true;
                        data["primaryKeyType"] = col._colType;
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
        std::vector<std::string> pkNames, pkTypes;
        for (size_t i = 1; i <= pkNumber; i++)
        {
            *client << "SELECT \
                pg_attribute.attname AS colname,\
                pg_type.typname AS typename,\
                pg_constraint.contype AS contype \
                FROM pg_constraint \
                INNER JOIN pg_class ON pg_constraint.conrelid = pg_class.oid \
                INNER JOIN pg_attribute ON pg_attribute.attrelid = pg_class.oid \
                AND pg_attribute.attnum = pg_constraint.conkey [ $1 ] \
                INNER JOIN pg_type ON pg_type.oid = pg_attribute.atttypid \
                WHERE pg_class.relname = $2 and pg_constraint.contype='p'"
                    << (int)i
                    << tableName
                    << Mode::Blocking >>
                [&](bool isNull, std::string colName, const std::string &type) {
                    if (isNull)
                        return;
                    //std::cout << "primary key name=" << colName << std::endl;
                    pkNames.push_back(colName);
                    for (auto &col : cols)
                    {
                        if (col._colName == colName)
                        {
                            col._isPrimaryKey = true;
                            pkTypes.push_back(col._colType);
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
    }

    data["columns"] = cols;
    std::ofstream headerFile(path + "/" + className + ".h", std::ofstream::out);
    std::ofstream sourceFile(path + "/" + className + ".cc", std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("model_h.csp");
    headerFile << templ->genText(data);
    templ = DrTemplateBase::newTemplate("model_cc.csp");
    sourceFile << templ->genText(data);
}
void create_model::createModelFromPG(const std::string &path, const DbClientPtr &client)
{
    *client << "SELECT a.oid,"
               "a.relname AS name,"
               "b.description AS comment "
               "FROM pg_class a "
               "LEFT OUTER JOIN pg_description b ON b.objsubid = 0 AND a.oid = b.objoid "
               "WHERE a.relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public') "
               "AND a.relkind = 'r' ORDER BY a.relname"
            << Mode::Blocking >>
        [&](bool isNull, size_t oid, const std::string &tableName, const std::string &comment) {
            if (!isNull)
            {
                std::cout << "table name:" << tableName << std::endl;
                createModelClassFromPG(path, client, tableName);
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
}
#endif

#if USE_MYSQL
void create_model::createModelClassFromMysql(const std::string &path, const DbClientPtr &client, const std::string &tableName)
{
    auto className = nameTransform(tableName, true);
    HttpViewData data;
    data["className"] = className;
    data["tableName"] = tableName;
    data["hasPrimaryKey"] = (int)0;
    data["primaryKeyName"] = "";
    data["dbName"] = _dbname;
    data["rdbms"] = std::string("mysql");
    std::vector<ColumnInfo> cols;
    int i = 0;
    *client << "desc " + tableName << Mode::Blocking >>
        [&](bool isNull, const std::string &field, const std::string &type, const std::string &isNullAble, const std::string &key, const std::string &defaultVal, const std::string &extra) {
            if (!isNull)
            {
                ColumnInfo info;
                info._index = i;
                info._dbType = "pg";
                info._colName = field;
                info._colTypeName = nameTransform(info._colName, true);
                info._colValName = nameTransform(info._colName, false);
                info._notNull = isNullAble == "YES" ? false : true;
                info._colDatabaseType = type;
                info._isPrimaryKey = key == "PRI" ? true : false;
                if (type.find("tinyint") == 0)
                {
                    info._colType = "int8_t";
                    info._colLength = 1;
                }
                else if (type.find("smallint") == 0)
                {
                    info._colType = "int16_t";
                    info._colLength = 2;
                }
                else if (type.find("int") == 0)
                {
                    info._colType = "int32_t";
                    info._colLength = 4;
                }
                else if (type.find("bigint") == 0)
                {
                    info._colType = "int64_t";
                    info._colLength = 8;
                }
                else if (type.find("float") == 0)
                {
                    info._colType = "float";
                    info._colLength = sizeof(float);
                }
                else if (type.find("double") == 0)
                {
                    info._colType = "double";
                    info._colLength = sizeof(double);
                }
                else if (type.find("date") == 0 || type.find("datetime") == 0 || type.find("timestamp") == 0)
                {
                    info._colType = "::trantor::Date";
                }
                else if (type.find("blob") != std::string::npos)
                {
                    info._colType = "std::vector<char>";
                }
                else
                {
                    info._colType = "std::string";
                }
                if (type.find("unsigned") != std::string::npos)
                {
                    info._colType = "u" + info._colType;
                }
                if (!defaultVal.empty())
                {
                    info._hasDefaultVal = true;
                }
                if (extra.find("auto_") == 0)
                {
                    info._isAutoVal = true;
                }
                cols.push_back(std::move(info));
                i++;
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
    std::vector<std::string> pkNames, pkTypes;
    for (auto col : cols)
    {
        if (col._isPrimaryKey)
        {
            pkNames.push_back(col._colName);
            pkTypes.push_back(col._colType);
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
    }
    data["columns"] = cols;
    std::ofstream headerFile(path + "/" + className + ".h", std::ofstream::out);
    std::ofstream sourceFile(path + "/" + className + ".cc", std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("model_h.csp");
    headerFile << templ->genText(data);
    templ = DrTemplateBase::newTemplate("model_cc.csp");
    sourceFile << templ->genText(data);
}
void create_model::createModelFromMysql(const std::string &path, const DbClientPtr &client)
{
    *client << "show tables" << Mode::Blocking >>
        [&](bool isNull, const std::string &tableName) {
            if (!isNull)
            {
                std::cout << "table name:" << tableName << std::endl;
                createModelClassFromMysql(path, client, tableName);
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
}
#endif

void create_model::createModel(const std::string &path, const Json::Value &config)
{
    auto dbType = config.get("rdbms", "no dbms").asString();
    std::transform(dbType.begin(), dbType.end(), dbType.begin(), tolower);
    if (dbType == "postgresql")
    {
#if USE_POSTGRESQL
        std::cout << "postgresql" << std::endl;
        auto host = config.get("host", "127.0.0.1").asString();
        auto port = config.get("port", 5432).asUInt();
        auto dbname = config.get("dbname", "").asString();
        if (dbname == "")
        {
            std::cerr << "Please configure dbname in " << path << "/model.json " << std::endl;
            exit(1);
        }
        _dbname = dbname;
        auto user = config.get("user", "").asString();
        if (user == "")
        {
            std::cerr << "Please configure user in " << path << "/model.json " << std::endl;
            exit(1);
        }
        auto password = config.get("passwd", "").asString();

        auto connStr = formattedString("host=%s port=%u dbname=%s user=%s", host.c_str(), port, dbname.c_str(), user.c_str());
        if (!password.empty())
        {
            connStr += " password=";
            connStr += password;
        }
        DbClientPtr client = drogon::orm::DbClient::newPgClient(connStr, 1);
        std::cout << "Connect to server..." << std::endl;
        std::cout << "Source files in the " << path << " folder will be overwritten, continue(y/n)?\n";
        auto in = getchar();
        if (in != 'Y' && in != 'y')
        {
            std::cout << "Abort!" << std::endl;
            exit(0);
        }
        auto tables = config["tables"];
        if (!tables || tables.size() == 0)
            createModelFromPG(path, client);
        else
        {
            for (int i = 0; i < (int)tables.size(); i++)
            {
                auto tableName = tables[i].asString();
                std::cout << "table name:" << tableName << std::endl;
                createModelClassFromPG(path, client, tableName);
            }
        }
#else
        std::cerr << "Drogon does not support PostgreSQL, please install PostgreSQL development environment before installing drogon" << std::endl;
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
            std::cerr << "Please configure dbname in " << path << "/model.json " << std::endl;
            exit(1);
        }
        _dbname = dbname;
        auto user = config.get("user", "").asString();
        if (user == "")
        {
            std::cerr << "Please configure user in " << path << "/model.json " << std::endl;
            exit(1);
        }
        auto password = config.get("passwd", "").asString();

        auto connStr = formattedString("host=%s port=%u dbname=%s user=%s", host.c_str(), port, dbname.c_str(), user.c_str());
        if (!password.empty())
        {
            connStr += " password=";
            connStr += password;
        }
        DbClientPtr client = drogon::orm::DbClient::newMysqlClient(connStr, 1);
        std::cout << "Connect to server..." << std::endl;
        std::cout << "Source files in the " << path << " folder will be overwritten, continue(y/n)?\n";
        auto in = getchar();
        if (in != 'Y' && in != 'y')
        {
            std::cout << "Abort!" << std::endl;
            exit(0);
        }
        auto tables = config["tables"];
        if (!tables || tables.size() == 0)
            createModelFromMysql(path, client);
        else
        {
            for (int i = 0; i < (int)tables.size(); i++)
            {
                auto tableName = tables[i].asString();
                std::cout << "table name:" << tableName << std::endl;
                createModelClassFromMysql(path, client, tableName);
            }
        }
#else
        std::cerr << "Drogon does not support Mysql, please install MariaDB development environment before installing drogon" << std::endl;
#endif
    }
    else if (dbType == "sqlite3")
    {
#if USE_SQLITE3
#endif
    }
    else if (dbType == "no dbms")
    {
        std::cerr << "Please configure Model in " << path << "/model.json " << std::endl;
        exit(1);
    }
    else
    {
        std::cerr << "Does not support " << dbType << std::endl;
        exit(1);
    }
}
void create_model::createModel(const std::string &path)
{
    DIR *dp;
    if ((dp = opendir(path.c_str())) == NULL)
    {
        std::cerr << "No such file or directory : " << path << std::endl;
        return;
    }
    closedir(dp);
    auto configFile = path + "/model.json";
    if (access(configFile.c_str(), 0) != 0)
    {
        std::cerr << "Config file " << configFile << " not found!" << std::endl;
        exit(1);
    }
    if (access(configFile.c_str(), R_OK) != 0)
    {
        std::cerr << "No permission to read config file " << configFile << std::endl;
        exit(1);
    }

    std::ifstream infile(configFile.c_str(), std::ifstream::in);
    if (infile)
    {
        Json::Value configJsonRoot;
        try
        {
            infile >> configJsonRoot;
            createModel(path, configJsonRoot);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Configuration file format error! in " << configFile << ":" << std::endl;
            std::cerr << exception.what() << std::endl;
            exit(1);
        }
    }
}

void create_model::handleCommand(std::vector<std::string> &parameters)
{
#if USE_ORM
    std::cout << "Create model" << std::endl;
    if (parameters.size() == 0)
    {
        std::cerr << "Missing Model path name!" << std::endl;
    }
    for (auto path : parameters)
    {
        createModel(path);
    }
#else
    std::cout << "No database can be found in your system, please install one first!" << std::endl;
    exit(1);
#endif
}
