/**
 *
 *  @file DotenvParser.cc
 *  @author akaahmedkamal
 *
 *  Copyright 2020, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "DotenvParser.h"
#include "drogon/utils/Utilities.h"

#include <iostream>

using namespace drogon;

DotenvParser::DotenvParser()
{
}
DotenvParser::~DotenvParser()
{
}
static std::string trim(const std::string &s, const char *c)
{
    std::string ss = s;
    ss.erase(s.find_last_not_of(c) + 1);
    ss.erase(0, s.find_first_not_of(c));
    return ss;
}
static std::string parseNamespace(const std::string &line)
{
    std::string ns;
    for (const auto &c : line)
    {
        if (c == '[')
        {
            continue;
        }
        if (c == ']')
        {
            break;
        }
        ns += c;
    }
    return ns;
}
static std::string removeInlineComments(const std::string &value)
{
    return drogon::utils::splitString(value, "#")[0];
}
static std::pair<std::string, std::string> parseKeyValue(const std::string &line)
{
    auto parts = drogon::utils::splitString(line, "=");
    if (parts.size() < 2)
    {
        return {"", ""};
    }
    return {parts[0], removeInlineComments(parts[1])};
}
static bool isEqual(const std::string &a, const std::string &b)
{
    std::string upperA = a;
    std::string upperB = b;
    std::transform(upperA.begin(), upperA.end(), upperA.begin(), ::toupper);
    std::transform(upperB.begin(), upperB.end(), upperB.begin(), ::toupper);
    return upperA == upperB;
}
void DotenvParser::parse(std::ifstream &infile)
{
    std::ostringstream content;
    if (infile.is_open())
    {
        content << infile.rdbuf();
    }
    infile.close();

    std::vector<std::string> lines = drogon::utils::splitString(content.str(), "\n");
    std::string ns;

    int listenersIndex = -1;
    int dbClientsIndex = -1;
    int redisClientsIndex = -1;

    for (const auto &line : lines)
    {
        if (line.empty())
        {
            continue;
        }
        std::string cleanLine = trim(line, " ");
        if (cleanLine[0] == '#')
        {
            continue;
        }
        if (cleanLine[0] == '[')
        {
            ns = parseNamespace(cleanLine);
            if (isEqual(ns, "listeners"))
            {
                listenersIndex++;
            }
            else if (isEqual(ns, "db_clients"))
            {
                dbClientsIndex++;
            }
            else if (isEqual(ns, "redis_clients"))
            {
                redisClientsIndex++;
            }
            continue;
        }
        auto pair = parseKeyValue(cleanLine);
        if (isEqual(ns, "app"))
        {
            if (isEqual(pair.first, "number_of_threads"))
            {
                configJsonValue_["app"]["number_of_threads"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "enable_session"))
            {
                configJsonValue_["app"]["enable_session"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "session_timeout"))
            {
                configJsonValue_["app"]["session_timeout"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "document_root"))
            {
                configJsonValue_["app"]["document_root"] = pair.second;
            }
            else if (isEqual(pair.first, "upload_path"))
            {
                configJsonValue_["app"]["upload_path"] = pair.second;
            }
            else if (isEqual(pair.first, "max_connections"))
            {
                configJsonValue_["app"]["max_connections"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "max_connections_per_ip"))
            {
                configJsonValue_["app"]["max_connections_per_ip"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "load_dynamic_views"))
            {
                configJsonValue_["app"]["load_dynamic_views"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "dynamic_views_output_path"))
            {
                configJsonValue_["app"]["dynamic_views_output_path"] = pair.second;
            }
            else if (isEqual(pair.first, "enable_unicode_escaping_in_json"))
            {
                configJsonValue_["app"]["enable_unicode_escaping_in_json"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "run_as_daemon"))
            {
                configJsonValue_["app"]["run_as_daemon"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "handle_sig_term"))
            {
                configJsonValue_["app"]["handle_sig_term"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "relaunch_on_error"))
            {
                configJsonValue_["app"]["relaunch_on_error"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "use_sendfile"))
            {
                configJsonValue_["app"]["use_sendfile"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "use_gzip"))
            {
                configJsonValue_["app"]["use_gzip"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "use_brotli"))
            {
                configJsonValue_["app"]["use_brotli"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "static_files_cache_time"))
            {
                configJsonValue_["app"]["static_files_cache_time"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "idle_connection_timeout"))
            {
                configJsonValue_["app"]["idle_connection_timeout"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "server_header_field"))
            {
                configJsonValue_["app"]["server_header_field"] = pair.second;
            }
            else if (isEqual(pair.first, "enable_server_header"))
            {
                configJsonValue_["app"]["enable_server_header"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "enable_date_header"))
            {
                configJsonValue_["app"]["enable_date_header"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "keepalive_requests"))
            {
                configJsonValue_["app"]["keepalive_requests"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "pipelining_requests"))
            {
                configJsonValue_["app"]["pipelining_requests"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "gzip_static"))
            {
                configJsonValue_["app"]["gzip_static"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "br_static"))
            {
                configJsonValue_["app"]["br_static"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "client_max_body_size"))
            {
                configJsonValue_["app"]["client_max_body_size"] = pair.second;
            }
            else if (isEqual(pair.first, "client_max_memory_body_size"))
            {
                configJsonValue_["app"]["client_max_memory_body_size"] = pair.second;
            }
            else if (isEqual(pair.first, "client_max_websocket_message_size"))
            {
                configJsonValue_["app"]["client_max_websocket_message_size"] = pair.second;
            }
            else if (isEqual(pair.first, "reuse_port"))
            {
                configJsonValue_["app"]["reuse_port"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "home_page"))
            {
                configJsonValue_["app"]["home_page"] = pair.second;
            }
            else if (isEqual(pair.first, "use_implicit_page"))
            {
                configJsonValue_["app"]["use_implicit_page"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "implicit_page"))
            {
                configJsonValue_["app"]["implicit_page"] = pair.second;
            }
        }
        else if (isEqual(ns, "log"))
        {
            if (isEqual(pair.first, "log_path"))
            {
                configJsonValue_["app"]["log"]["log_path"] = pair.second;
            }
            else if (isEqual(pair.first, "logfile_base_name"))
            {
                configJsonValue_["app"]["log"]["logfile_base_name"] = pair.second;
            }
            else if (isEqual(pair.first, "log_size_limit"))
            {
                configJsonValue_["app"]["log"]["log_size_limit"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "log_level"))
            {
                configJsonValue_["app"]["log"]["log_level"] = pair.second;
            }
        }
        else if (isEqual(ns, "ssl"))
        {
            if (isEqual(pair.first, "key"))
            {
                configJsonValue_["ssl"]["key"] = pair.second;
            }
            else if (isEqual(pair.first, "cert"))
            {
                configJsonValue_["ssl"]["cert"] = pair.second;
            }
        }
        else if (isEqual(ns, "listeners"))
        {
            if (isEqual(pair.first, "address"))
            {
                configJsonValue_["listeners"][listenersIndex]["address"] = pair.second;
            }
            else if (isEqual(pair.first, "port"))
            {
                configJsonValue_["listeners"][listenersIndex]["port"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "https"))
            {
                configJsonValue_["listeners"][listenersIndex]["https"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "cert"))
            {
                configJsonValue_["listeners"][listenersIndex]["cert"] = pair.second;
            }
            else if (isEqual(pair.first, "key"))
            {
                configJsonValue_["listeners"][listenersIndex]["key"] = pair.second;
            }
            else if (isEqual(pair.first, "use_old_tls"))
            {
                configJsonValue_["listeners"][listenersIndex]["use_old_tls"] = isEqual(pair.second, "true");
            }
        }
        else if (isEqual(ns, "db_clients"))
        {
            if (isEqual(pair.first, "rdbms"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["rdbms"] = pair.second;
            }
            else if (isEqual(pair.first, "host"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["host"] = pair.second;
            }
            else if (isEqual(pair.first, "port"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["port"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "dbname"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["dbname"] = pair.second;
            }
            else if (isEqual(pair.first, "user"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["user"] = pair.second;
            }
            else if (isEqual(pair.first, "passwd"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["passwd"] = pair.second;
            }
            else if (isEqual(pair.first, "password"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["password"] = pair.second;
            }
            else if (isEqual(pair.first, "connection_number"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["connection_number"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "number_of_connections"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["number_of_connections"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "name"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["name"] = pair.second;
            }
            else if (isEqual(pair.first, "filename"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["filename"] = pair.second;
            }
            else if (isEqual(pair.first, "is_fast"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["is_fast"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "characterSet"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["characterSet"] = pair.second;
            }
            else if (isEqual(pair.first, "client_encoding"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["client_encoding"] = pair.second;
            }
            else if (isEqual(pair.first, "timeout"))
            {
                configJsonValue_["db_clients"][dbClientsIndex]["timeout"] = std::stod(pair.second);
            }
        }
        else if (isEqual(ns, "redis_clients"))
        {
            if (isEqual(pair.first, "host"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["host"] = pair.second;
            }
            else if (isEqual(pair.first, "port"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["port"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "passwd"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["passwd"] = pair.second;
            }
            else if (isEqual(pair.first, "password"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["password"] = pair.second;
            }
            else if (isEqual(pair.first, "connection_number"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["connection_number"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "number_of_connections"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["number_of_connections"] = std::stoi(pair.second);
            }
            else if (isEqual(pair.first, "name"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["name"] = pair.second;
            }
            else if (isEqual(pair.first, "is_fast"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["is_fast"] = isEqual(pair.second, "true");
            }
            else if (isEqual(pair.first, "timeout"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["timeout"] = std::stod(pair.second);
            }
            else if (isEqual(pair.first, "db"))
            {
                configJsonValue_["redis_clients"][redisClientsIndex]["db"] = std::stoi(pair.second);
            }
        }
    }
}
