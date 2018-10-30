/**
 *
 *  create_dao.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#include "create_dao.h"
#include "cmd.h"

#include <drogon/config.h>
#include <drogon/utils/Utilities.h>
#if USE_POSTGRESQL
#include <drogon/orm/PgClient.h>
#endif
#include <drogon/HttpViewData.h>
#include <drogon/DrTemplateBase.h>
#include <json/json.h>
#include <iostream>
#include <fstream>
#include <regex>

#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fstream>
#include <unistd.h>

using namespace drogon_ctl;
using namespace drogon::orm;

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
void create_dao::createDAOClassFromPG(const std::string &path, PgClient &client, const std::string &tableName)
{
    auto className = nameTransform(tableName, true);
    HttpViewData data;
    data["className"] = className;
    data["tableName"] = tableName;
    data["hasPrimaryKey"] = true;
    data["primaryKeyName"] = "";
    data["dbName"] = _dbname;
    std::ofstream headerFile(path + "/" + className + ".h", std::ofstream::out);
    std::ofstream sourceFile(path + "/" + className + ".cc", std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("dao_h.csp");
    headerFile << templ->genText(data);
    templ = DrTemplateBase::newTemplate("dao_cc.csp");
    sourceFile << templ->genText(data);
}
void create_dao::createDAOFromPG(const std::string &path, PgClient &client)
{
    client << "SELECT a.oid,"
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
                createDAOClassFromPG(path, client, tableName);
            }
        } >>
        [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
            exit(1);
        };
}
#endif
void create_dao::createDAO(const std::string &path, const Json::Value &config)
{
    auto dbType = config.get("rdbms", "No dbms").asString();
    if (dbType == "postgreSQL")
    {
#if USE_POSTGRESQL
        std::cout << "postgresql" << std::endl;
        auto host = config.get("host", "127.0.0.1").asString();
        auto port = config.get("port", 5432).asUInt();
        auto dbname = config.get("dbname", "").asString();
        if (dbname == "")
        {
            std::cerr << "Please configure dbname in " << path << "/dao.json " << std::endl;
            exit(1);
        }
        _dbname = dbname;
        auto user = config.get("user", "").asString();
        if (user == "")
        {
            std::cerr << "Please configure user in " << path << "/dao.json " << std::endl;
            exit(1);
        }
        auto password = config.get("passwd", "").asString();

        auto connStr = formattedString("host=%s port=%u dbname=%s user=%s", host.c_str(), port, dbname.c_str(), user.c_str());
        if (!password.empty())
        {
            connStr += " password=";
            connStr += password;
        }
        PgClient client(connStr, 1);
        std::cout << "Connect to server..." << std::endl;
        sleep(1);
        createDAOFromPG(path, client);
#endif
    }
    else if (dbType == "No dbms")
    {
        std::cerr << "Please configure DAO in " << path << "/dao.json " << std::endl;
        exit(1);
    }
    else
    {
        std::cerr << "Does not support " << dbType << std::endl;
        exit(1);
    }
}
void create_dao::createDAO(const std::string &path)
{
    DIR *dp;
    if ((dp = opendir(path.c_str())) == NULL)
    {
        std::cerr << "No such file or directory : " << path << std::endl;
        return;
    }
    closedir(dp);
    auto configFile = path + "/dao.json";
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
            createDAO(path, configJsonRoot);
        }
        catch (const std::exception &exception)
        {
            std::cerr << "Configuration file format error! in " << configFile << ":" << std::endl;
            std::cerr << exception.what() << std::endl;
            exit(1);
        }
    }
}

void create_dao::handleCommand(std::vector<std::string> &parameters)
{
    std::cout << "Create dao" << std::endl;
    if (parameters.size() == 0)
    {
        std::cerr << "Missing DAO path name!" << std::endl;
    }
    for (auto path : parameters)
    {
        createDAO(path);
    }
}