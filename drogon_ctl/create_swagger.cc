/**
 *
 *  create_swagger.cc
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

#include "create_swagger.h"
#include <drogon/DrTemplateBase.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#include <dirent.h>
#else
#include <io.h>
#include <direct.h>
#endif
#include <fstream>
#include <set>
#include <unordered_map>
#include <regex>

using namespace drogon_ctl;

namespace
{
enum class ContextType
{
    kNamespace,
    kClass
};

struct ContextNode
{
    ContextType type;
    std::string name;
};

static std::string trim(const std::string &s)
{
    size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])))
    {
        ++begin;
    }
    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
    {
        --end;
    }
    return s.substr(begin, end - begin);
}

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static bool startsWith(const std::string &s,
                       size_t start,
                       const std::string &prefix)
{
    if (start + prefix.size() > s.size())
    {
        return false;
    }
    return s.compare(start, prefix.size(), prefix) == 0;
}

static std::string stripCppComments(const std::string &content)
{
    std::string out;
    out.reserve(content.size());
    bool inString = false;
    bool inChar = false;
    bool inRawString = false;
    size_t rawEndPos = 0;

    for (size_t i = 0; i < content.size(); ++i)
    {
        if (inRawString)
        {
            out.push_back(content[i]);
            if (i + 1 >= rawEndPos)
            {
                inRawString = false;
            }
            continue;
        }

        if (!inString && !inChar && startsWith(content, i, "R\""))
        {
            auto open = content.find('(', i + 2);
            if (open != std::string::npos)
            {
                auto marker = content.substr(i + 2, open - (i + 2));
                auto endMarker = ")" + marker + "\"";
                auto endPos = content.find(endMarker, open + 1);
                if (endPos != std::string::npos)
                {
                    rawEndPos = endPos + endMarker.size() - 1;
                    inRawString = true;
                    out.push_back(content[i]);
                    continue;
                }
            }
        }

        if (!inString && !inChar && i + 1 < content.size() && content[i] == '/' &&
            content[i + 1] == '/')
        {
            while (i < content.size() && content[i] != '\n')
            {
                ++i;
            }
            if (i < content.size())
            {
                out.push_back(content[i]);
            }
            continue;
        }

        if (!inString && !inChar && i + 1 < content.size() && content[i] == '/' &&
            content[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < content.size() &&
                   !(content[i] == '*' && content[i + 1] == '/'))
            {
                ++i;
            }
            ++i;
            continue;
        }

        out.push_back(content[i]);
        if (!inChar && content[i] == '"' && (i == 0 || content[i - 1] != '\\'))
        {
            inString = !inString;
        }
        else if (!inString && content[i] == '\'' &&
                 (i == 0 || content[i - 1] != '\\'))
        {
            inChar = !inChar;
        }
    }

    return out;
}

static bool parseMacroInvocation(const std::string &content,
                                 size_t openParenPos,
                                 std::string &argsContent,
                                 size_t &endPos)
{
    if (openParenPos >= content.size() || content[openParenPos] != '(')
    {
        return false;
    }
    int depth = 0;
    bool inString = false;
    bool inChar = false;
    for (size_t i = openParenPos; i < content.size(); ++i)
    {
        auto ch = content[i];
        if (!inChar && ch == '"' && (i == 0 || content[i - 1] != '\\'))
        {
            inString = !inString;
            continue;
        }
        if (!inString && ch == '\'' && (i == 0 || content[i - 1] != '\\'))
        {
            inChar = !inChar;
            continue;
        }
        if (inString || inChar)
        {
            continue;
        }

        if (ch == '(')
        {
            ++depth;
        }
        else if (ch == ')')
        {
            --depth;
            if (depth == 0)
            {
                argsContent = content.substr(openParenPos + 1,
                                             i - openParenPos - 1);
                endPos = i;
                return true;
            }
        }
    }

    return false;
}

static std::vector<std::string> splitTopLevelArgs(const std::string &args)
{
    std::vector<std::string> out;
    std::string current;
    int parenDepth = 0;
    int braceDepth = 0;
    int bracketDepth = 0;
    bool inString = false;
    bool inChar = false;

    for (size_t i = 0; i < args.size(); ++i)
    {
        char ch = args[i];

        if (!inChar && ch == '"' && (i == 0 || args[i - 1] != '\\'))
        {
            inString = !inString;
            current.push_back(ch);
            continue;
        }
        if (!inString && ch == '\'' && (i == 0 || args[i - 1] != '\\'))
        {
            inChar = !inChar;
            current.push_back(ch);
            continue;
        }
        if (inString || inChar)
        {
            current.push_back(ch);
            continue;
        }

        if (ch == '(')
            ++parenDepth;
        else if (ch == ')')
            --parenDepth;
        else if (ch == '{')
            ++braceDepth;
        else if (ch == '}')
            --braceDepth;
        else if (ch == '[')
            ++bracketDepth;
        else if (ch == ']')
            --bracketDepth;

        if (ch == ',' && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0)
        {
            out.emplace_back(trim(current));
            current.clear();
            continue;
        }

        current.push_back(ch);
    }
    if (!trim(current).empty())
    {
        out.emplace_back(trim(current));
    }
    return out;
}

static std::string parseCppStringLiteral(const std::string &token)
{
    auto s = trim(token);
    if (s.empty())
    {
        return "";
    }

    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 1; i + 1 < s.size(); ++i)
        {
            if (s[i] == '\\' && i + 1 < s.size() - 1)
            {
                ++i;
            }
            out.push_back(s[i]);
        }
        return out;
    }

    if (startsWith(s, 0, "R\"") && s.size() >= 4)
    {
        auto open = s.find('(');
        auto close = s.rfind(')');
        if (open != std::string::npos && close != std::string::npos && close > open)
        {
            return s.substr(open + 1, close - open - 1);
        }
    }

    return "";
}

static std::string joinClassPath(const std::vector<ContextNode> &context,
                                 const std::string &methodToken)
{
    std::vector<std::string> ns;
    std::string className;
    for (const auto &node : context)
    {
        if (node.type == ContextType::kNamespace)
        {
            ns.push_back(node.name);
        }
        else if (node.type == ContextType::kClass)
        {
            className = node.name;
        }
    }

    if (className.empty())
    {
        auto pos = methodToken.rfind("::");
        if (pos != std::string::npos)
        {
            className = methodToken.substr(0, pos);
        }
    }

    if (className.empty())
    {
        return "";
    }

    std::string path = "/";
    for (const auto &name : ns)
    {
        auto parts = drogon::utils::splitString(name, "::");
        for (const auto &p : parts)
        {
            if (!p.empty())
            {
                path += p;
                path.push_back('/');
            }
        }
    }
    path += className;
    return path;
}

static std::set<std::string> parseHttpMethods(const std::vector<std::string> &args,
                                              size_t startIndex)
{
    static const std::unordered_map<std::string, std::string> methodMap = {
        {"get", "get"},
        {"post", "post"},
        {"put", "put"},
        {"delete", "delete"},
        {"patch", "patch"},
        {"head", "head"},
        {"options", "options"}};

    std::set<std::string> methods;
    for (size_t i = startIndex; i < args.size(); ++i)
    {
        auto token = trim(args[i]);
        if (token.empty())
        {
            continue;
        }
        auto pos = token.rfind("::");
        if (pos != std::string::npos)
        {
            token = token.substr(pos + 2);
        }
        token = toLower(token);
        auto it = methodMap.find(token);
        if (it != methodMap.end())
        {
            methods.insert(it->second);
        }
    }

    if (methods.empty())
    {
        methods.insert("get");
    }
    return methods;
}

static std::string normalizePathForSwagger(std::string path)
{
    auto queryPos = path.find('?');
    if (queryPos != std::string::npos)
    {
        path = path.substr(0, queryPos);
    }
    if (path.empty())
    {
        path = "/";
    }

    std::string converted;
    converted.reserve(path.size());
    int autoParamIndex = 1;
    for (size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == '{')
        {
            auto end = path.find('}', i + 1);
            if (end == std::string::npos)
            {
                converted.push_back(path[i]);
                continue;
            }
            auto inside = trim(path.substr(i + 1, end - i - 1));
            std::string paramName;
            if (inside.empty())
            {
                paramName = "param" + std::to_string(autoParamIndex++);
            }
            else if (std::all_of(inside.begin(), inside.end(), [](unsigned char ch) {
                         return std::isdigit(ch);
                     }))
            {
                paramName = "arg" + inside;
            }
            else
            {
                paramName = inside;
            }
            converted += "{" + paramName + "}";
            i = end;
            continue;
        }
        converted.push_back(path[i]);
    }

    std::string compact;
    compact.reserve(converted.size());
    bool lastSlash = false;
    for (auto ch : converted)
    {
        if (ch == '/')
        {
            if (!lastSlash)
            {
                compact.push_back(ch);
            }
            lastSlash = true;
        }
        else
        {
            compact.push_back(ch);
            lastSlash = false;
        }
    }

    if (compact.empty())
    {
        return "/";
    }
    return compact;
}

static std::string extractMethodName(const std::string &methodToken)
{
    auto token = trim(methodToken);
    auto pos = token.rfind("::");
    if (pos == std::string::npos)
    {
        return token;
    }
    return token.substr(pos + 2);
}

static std::string extractClassName(const std::vector<ContextNode> &context,
                                    const std::string &methodToken)
{
    std::string className;
    std::vector<std::string> namespaces;
    for (const auto &node : context)
    {
        if (node.type == ContextType::kNamespace)
        {
            namespaces.push_back(node.name);
        }
        else if (node.type == ContextType::kClass)
        {
            className = node.name;
        }
    }

    if (className.empty())
    {
        auto token = trim(methodToken);
        auto pos = token.rfind("::");
        if (pos != std::string::npos)
        {
            className = token.substr(0, pos);
        }
    }

    if (className.empty())
    {
        return "Controller";
    }

    if (namespaces.empty())
    {
        return className;
    }

    std::string full;
    for (const auto &ns : namespaces)
    {
        auto parts = drogon::utils::splitString(ns, "::");
        for (const auto &p : parts)
        {
            if (!p.empty())
            {
                if (!full.empty())
                {
                    full += "::";
                }
                full += p;
            }
        }
    }
    if (!full.empty())
    {
        full += "::";
    }
    full += className;
    return full;
}

static std::string sanitizeOperationId(std::string s)
{
    for (auto &c : s)
    {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
        {
            c = '_';
        }
    }
    std::string compact;
    compact.reserve(s.size());
    bool lastUnderscore = false;
    for (auto c : s)
    {
        if (c == '_')
        {
            if (!lastUnderscore)
            {
                compact.push_back(c);
            }
            lastUnderscore = true;
        }
        else
        {
            compact.push_back(c);
            lastUnderscore = false;
        }
    }
    if (compact.empty())
    {
        return "operation";
    }
    return compact;
}

static std::vector<std::string> extractPathParams(const std::string &routePath)
{
    std::vector<std::string> params;
    std::regex pathParamRegex(R"(\{([^\}/]+)\})");
    for (std::sregex_iterator it(routePath.begin(), routePath.end(), pathParamRegex),
         end;
         it != end;
         ++it)
    {
        auto p = (*it)[1].str();
        if (!p.empty() &&
            std::find(params.begin(), params.end(), p) == params.end())
        {
            params.push_back(std::move(p));
        }
    }
    return params;
}

static void fillSwaggerOperation(Json::Value &operation,
                                 const std::string &controllerName,
                                 const std::string &methodName,
                                 const std::string &routePath,
                                 const std::string &httpMethod)
{
    operation["summary"] = methodName + " " + routePath;
    operation["tags"] = Json::arrayValue;
    operation["tags"].append(controllerName);
    operation["operationId"] =
        sanitizeOperationId(controllerName + "_" + methodName + "_" + httpMethod);

    auto params = extractPathParams(routePath);
    if (!params.empty())
    {
        operation["parameters"] = Json::arrayValue;
        for (const auto &param : params)
        {
            Json::Value p;
            p["name"] = param;
            p["in"] = "path";
            p["required"] = true;
            p["type"] = "string";
            operation["parameters"].append(std::move(p));
        }
    }

    operation["responses"]["200"]["description"] = "OK";
}

static void collectEndpointsFromHeader(const std::string &headerFile,
                                       Json::Value &paths)
{
    std::ifstream infile(utils::toNativePath(headerFile), std::ifstream::binary);
    if (!infile)
    {
        std::cerr << "Warning: can't open the header file: " << headerFile << "\n";
        return;
    }

    std::string content((std::istreambuf_iterator<char>(infile)),
                        std::istreambuf_iterator<char>());
    auto noComment = stripCppComments(content);

    std::string namespacePrefix;
    {
        static const std::regex nsDeclRegex(
            R"(namespace\s+([A-Za-z_][A-Za-z0-9_:]*)\s*\{)");
        std::smatch match;
        if (std::regex_search(noComment, match, nsDeclRegex))
        {
            namespacePrefix = match[1].str();
        }
    }

    auto buildControllerIdentity =
        [&namespacePrefix](const std::string &methodToken,
                           std::string &controllerName,
                           std::string &basePath) {
            auto token = trim(methodToken);
            auto methodSep = token.rfind("::");
            if (methodSep == std::string::npos)
            {
                controllerName = "Controller";
                basePath = "/Controller";
                return;
            }

            auto classPart = token.substr(0, methodSep);
            std::vector<std::string> nsParts;
            std::string className = classPart;
            auto classSep = classPart.rfind("::");
            if (classSep != std::string::npos)
            {
                className = classPart.substr(classSep + 2);
                auto explicitNs = classPart.substr(0, classSep);
                nsParts = drogon::utils::splitString(explicitNs, "::");
            }
            else if (!namespacePrefix.empty())
            {
                nsParts = drogon::utils::splitString(namespacePrefix, "::");
            }

            controllerName.clear();
            for (const auto &p : nsParts)
            {
                if (p.empty())
                    continue;
                if (!controllerName.empty())
                {
                    controllerName += "::";
                }
                controllerName += p;
            }
            if (!controllerName.empty())
            {
                controllerName += "::";
            }
            controllerName += className;

            basePath = "/";
            for (const auto &p : nsParts)
            {
                if (!p.empty())
                {
                    basePath += p;
                    basePath.push_back('/');
                }
            }
            basePath += className;
        };

    size_t pos = 0;
    while (pos < noComment.size())
    {
        auto methodPos = noComment.find("METHOD_ADD(", pos);
        auto addToPos = noComment.find("ADD_METHOD_TO(", pos);

        bool isMethodAdd = false;
        size_t macroPos = std::string::npos;
        if (methodPos == std::string::npos)
        {
            macroPos = addToPos;
        }
        else if (addToPos == std::string::npos)
        {
            macroPos = methodPos;
            isMethodAdd = true;
        }
        else if (methodPos < addToPos)
        {
            macroPos = methodPos;
            isMethodAdd = true;
        }
        else
        {
            macroPos = addToPos;
        }

        if (macroPos == std::string::npos)
        {
            break;
        }

        const std::string macroName = isMethodAdd ? "METHOD_ADD" : "ADD_METHOD_TO";
        std::string argsText;
        size_t endPos = macroPos;
        if (parseMacroInvocation(noComment,
                                 macroPos + macroName.size(),
                                 argsText,
                                 endPos))
        {
            auto args = splitTopLevelArgs(argsText);
            if (args.size() >= 2)
            {
                std::string controllerName;
                std::string routePath;
                auto methodName = extractMethodName(args[0]);

                if (isMethodAdd)
                {
                    std::string basePath;
                    buildControllerIdentity(args[0], controllerName, basePath);
                    auto pattern = parseCppStringLiteral(args[1]);
                    if (!basePath.empty())
                    {
                        if (pattern.empty())
                        {
                            routePath = basePath;
                        }
                        else if (pattern.front() == '/')
                        {
                            routePath = basePath + pattern;
                        }
                        else
                        {
                            routePath = basePath + "/" + pattern;
                        }
                    }
                }
                else
                {
                    std::string basePath;
                    buildControllerIdentity(args[0], controllerName, basePath);
                    auto pathExpr = parseCppStringLiteral(args[1]);
                    if (!pathExpr.empty())
                    {
                        if (pathExpr.front() != '/')
                        {
                            pathExpr = "/" + pathExpr;
                        }
                        routePath = pathExpr;
                    }
                }

                if (!routePath.empty())
                {
                    routePath = normalizePathForSwagger(routePath);
                    auto methods = parseHttpMethods(args, 2);
                    for (const auto &m : methods)
                    {
                        fillSwaggerOperation(paths[routePath][m],
                                             controllerName,
                                             methodName,
                                             routePath,
                                             m);
                    }
                }
            }
            pos = endPos + 1;
        }
        else
        {
            pos = macroPos + 1;
        }
    }
}
}  // namespace

static void forEachControllerHeaderIn(
    std::string strPath,
    const std::function<void(const std::string &)> &cb)
{
    while (!strPath.empty())
    {
        char cEnd = *strPath.rbegin();
        if (cEnd == '\\' || cEnd == '/')
        {
            strPath = strPath.substr(0, strPath.length() - 1);
        }
        else
        {
            break;
        }
    }

    if (strPath.empty() || strPath == (".") || strPath == (".."))
        return;

    std::error_code ec;
    std::filesystem::path fsPath(strPath);
    if (!std::filesystem::exists(fsPath, ec) ||
        !std::filesystem::is_directory(fsPath, ec))
    {
        return;
    }

    std::filesystem::recursive_directory_iterator it(
        fsPath,
        std::filesystem::directory_options::skip_permission_denied,
        ec);
    std::filesystem::recursive_directory_iterator end;
    while (it != end)
    {
        if (ec)
        {
            ec.clear();
            it.increment(ec);
            continue;
        }

        if (it->is_regular_file(ec))
        {
            auto ext = toLower(it->path().extension().string());
            if (ext == ".h" || ext == ".hpp")
            {
                cb(it->path().string());
            }
        }

        ec.clear();
        it.increment(ec);
    }
}

static std::string makeSwaggerDocument(const Json::Value &config,
                                       const std::string &controllersPath)
{
    Json::Value ret;
    ret["swagger"] = "2.0";
    if (config.isMember("info") && config["info"].isObject())
    {
        ret["info"] = config["info"];
    }
    else
    {
        ret["info"]["title"] = "Drogon API";
        ret["info"]["version"] = "1.0.0";
    }

    Json::Value paths(Json::objectValue);
    forEachControllerHeaderIn(controllersPath, [&paths](const std::string &header) {
        collectEndpointsFromHeader(header, paths);
    });

    ret["paths"] = std::move(paths);
    return ret.toStyledString();
}

static void createSwaggerHeader(const std::string &path,
                                const Json::Value &config)
{
    drogon::HttpViewData data;
    data["docs_url"] = config.get("docs_url", "/swagger").asString();
    std::ofstream ctlHeader(path + "/SwaggerCtrl.h", std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("swagger_h.csp");
    ctlHeader << templ->genText(data);
}

static void createSwaggerSource(const std::string &path,
                                const Json::Value &config,
                                const std::string &controllersPath)
{
    drogon::HttpViewData data;
    data["docs_string"] = makeSwaggerDocument(config, controllersPath);

    std::ofstream ctlSource(path + "/SwaggerCtrl.cc", std::ofstream::out);
    auto templ = DrTemplateBase::newTemplate("swagger_cc.csp");
    ctlSource << templ->genText(data);
}

static void createSwagger(const std::string &path)
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
    auto configFile = path + "/swagger.json";
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
            std::filesystem::path projectRoot =
                std::filesystem::path(path).parent_path();
            auto controllersPath =
                configJsonRoot.get("controllers_path", "controllers").asString();
            std::filesystem::path controllersFsPath(controllersPath);
            if (controllersFsPath.is_relative())
            {
                controllersFsPath = projectRoot / controllersFsPath;
            }

            createSwaggerHeader(path, configJsonRoot);
            createSwaggerSource(path,
                                configJsonRoot,
                                controllersFsPath.string());
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
void create_swagger::handleCommand(std::vector<std::string> &parameters)
{
    if (parameters.size() < 1)
    {
        std::cout << "please input a path to create the swagger controller"
                  << std::endl;
        exit(1);
    }
    auto path = parameters[0];
    createSwagger(path);
}

// See create.cc for rationale.
template class drogon::DrObject<drogon_ctl::create_swagger>;
