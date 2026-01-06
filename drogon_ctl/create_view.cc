/**
 *
 *  @file create_view.cc
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "create_view.h"
#include "cmd.h"
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <regex>

static const std::string cxx_include = "<%inc";
static const std::string cxx_end = "%>";
static const std::string cxx_lang = "<%c++";
static const std::string cxx_view_data = "@@";
static const std::string cxx_output = "$$";
static const std::string cxx_val_start = "[[";
static const std::string cxx_val_end = "]]";
static const std::string sub_view_start = "<%view";
static const std::string sub_view_end = "%>";

enum class CspLeftTagType
{
    kCxxStart,
    kCxxValStart,
    kSubViewStart,
};

using namespace drogon_ctl;

static std::string &replace_all(std::string &str,
                                const std::string &old_value,
                                const std::string &new_value)
{
    std::string::size_type pos(0);
    while (true)
    {
        // std::cout<<str<<endl;
        // std::cout<<"pos="<<pos<<endl;
        if ((pos = str.find(old_value, pos)) != std::string::npos)
        {
            str = str.replace(pos, old_value.length(), new_value);
            pos += new_value.length() - old_value.length();
            ++pos;
        }
        else
            break;
    }
    return str;
}

static std::string parseParameter(const std::string &str)
{
    auto iter = str.begin();
    while (iter != str.end() && *iter == ' ')
        ++iter;
    auto keyword_start = iter;
    while (iter != str.end() && *iter != ' ')
        ++iter;
    return std::string(keyword_start, iter);
}

static void parseCxxLine(std::ofstream &oSrcFile,
                         const std::string &line,
                         const std::string &streamName,
                         const std::string &viewDataName)
{
    if (line.length() > 0)
    {
        std::string tmp = line;
        replace_all(tmp, cxx_output, streamName);
        replace_all(tmp, cxx_view_data, viewDataName);
        oSrcFile << tmp << "\n";
    }
}

static void outputVal(std::ofstream &oSrcFile,
                      const std::string &streamName,
                      const std::string &viewDataName,
                      const std::string &keyName)
{
    oSrcFile << "{\n";
    oSrcFile << "    auto & val=" << viewDataName << "[\"" << keyName
             << "\"];\n";
    oSrcFile << "    if(val.type()==typeid(const char *)){\n";
    oSrcFile << "        " << streamName
             << "<<*(std::any_cast<const char *>(&val));\n";
    oSrcFile << "    }else "
                "if(val.type()==typeid(std::string)||val.type()==typeid(const "
                "std::string)){\n";
    oSrcFile << "        " << streamName
             << "<<*(std::any_cast<const std::string>(&val));\n";
    oSrcFile << "    }\n";
    oSrcFile << "}\n";
}

static void outputSubView(std::ofstream &oSrcFile,
                          const std::string &streamName,
                          const std::string &viewDataName,
                          const std::string &keyName)
{
    oSrcFile << "{\n";
    oSrcFile << "    auto templ=DrTemplateBase::newTemplate(\"" << keyName
             << "\");\n";
    oSrcFile << "    if(templ){\n";
    oSrcFile << "      " << streamName << "<< templ->genText(" << viewDataName
             << ");\n";
    oSrcFile << "    }\n";
    oSrcFile << "}\n";
}

static void outputText(std::ofstream &oSrcFile,
                       std::string line,
                       const std::string &streamName,
                       bool do_return)
{
    if (line.length() == 0)
    {
        if (do_return)
        {
            oSrcFile << "\t" << streamName << " << \"\\n\";\n";
        }
        return;
    }

    replace_all(line, "\\", "\\\\");
    replace_all(line, "\"", "\\\"");
    oSrcFile << "\t" << streamName << " << \"" << line;

    if (do_return)
        oSrcFile << "\\n\";\n";
    else
        oSrcFile << "\";\n";
}

// if not found, second will be std::string::npos.
static std::pair<CspLeftTagType, std::string::size_type> findFirstLeftTag(
    const std::string &text)
{
    std::pair<CspLeftTagType, std::string::size_type> find_result[] = {
        {CspLeftTagType{}, std::string::npos},
        {CspLeftTagType::kCxxStart, text.find(cxx_lang)},
        {CspLeftTagType::kCxxValStart, text.find(cxx_val_start)},
        {CspLeftTagType::kSubViewStart, text.find(sub_view_start)}};

    auto &result = find_result[0];
    for (const auto &i : find_result)
    {
        if (i.second == std::string::npos)
            continue;

        if (i.second <= result.second)
            result = i;
    }
    return result;
}

static void parseLine(std::ofstream &oSrcFile,
                      std::string &line,
                      const std::string &streamName,
                      const std::string &viewDataName,
                      int &cxx_flag)
{
    std::string::size_type pos(0);
    // std::cout<<line<<"("<<line.length()<<")\n";
    if (line.length() > 0 && line[line.length() - 1] == '\r')
    {
        line.resize(line.length() - 1);
    }

    if (cxx_flag == 0)
    {
        auto found_tag = findFirstLeftTag(line);
        pos = found_tag.second;

        if (found_tag.second == std::string::npos)
        {
            // line dose not contain any tags
            outputText(oSrcFile, line, streamName, true);
            return;
        }

        if (pos != 0)
        {
            // [output here]<%c++ ...%>...
            std::string oldLine = line.substr(0, pos);
            outputText(oSrcFile, oldLine, streamName, false);
        }

        if (found_tag.first == CspLeftTagType::kCxxStart)
        {
            std::string newLine = line.substr(pos + cxx_lang.length());
            cxx_flag = 1;
            if (newLine.length() > 0)
                parseLine(
                    oSrcFile, newLine, streamName, viewDataName, cxx_flag);
        }
        else if (found_tag.first == CspLeftTagType::kCxxValStart)
        {
            std::string newLine = line.substr(pos + cxx_val_start.length());
            if ((pos = newLine.find(cxx_val_end)) != std::string::npos)
            {
                std::string keyName = newLine.substr(0, pos);
                auto i = keyName.begin();
                outputVal(oSrcFile,
                          streamName,
                          viewDataName,
                          parseParameter(keyName));

                std::string tailLine =
                    newLine.substr(pos + cxx_val_end.length());
                parseLine(
                    oSrcFile, tailLine, streamName, viewDataName, cxx_flag);
            }
            else
            {
                std::cerr << "format err!" << std::endl;
                exit(1);
            }
        }
        else if (found_tag.first == CspLeftTagType::kSubViewStart)
        {
            std::string newLine = line.substr(pos + sub_view_start.length());
            if ((pos = newLine.find(sub_view_end)) != std::string::npos)
            {
                std::string keyName = newLine.substr(0, pos);
                outputSubView(oSrcFile,
                              streamName,
                              viewDataName,
                              parseParameter(keyName));

                std::string tailLine =
                    newLine.substr(pos + sub_view_end.length());
                parseLine(
                    oSrcFile, tailLine, streamName, viewDataName, cxx_flag);
            }
            else
            {
                std::cerr << "format err!" << std::endl;
                exit(1);
            }
        }
    }
    else
    {
        if ((pos = line.find(cxx_end)) != std::string::npos)
        {
            std::string cxx_part = line.substr(0, pos);
            parseCxxLine(oSrcFile, cxx_part, streamName, viewDataName);

            std::string tailLine = line.substr(pos + cxx_end.length());
            cxx_flag = 0;
            if (tailLine.length() > 0)
                parseLine(
                    oSrcFile, tailLine, streamName, viewDataName, cxx_flag);
        }
        else
        {
            parseCxxLine(oSrcFile, line, streamName, viewDataName);
        }
    }
}

void create_view::handleCommand(std::vector<std::string> &parameters)
{
    for (auto iter = parameters.begin(); iter != parameters.end();)
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
        else if (file == "-n" || file == "--namespace")
        {
            iter = parameters.erase(iter);
            if (iter != parameters.end())
            {
                namespaces_ = utils::splitString(*iter, "::");
                iter = parameters.erase(iter);
            }
            continue;
        }
        else if (file == "--path-to-namespace")
        {
            iter = parameters.erase(iter);
            pathToNamespaceFlag_ = true;
            continue;
        }
        else if (file[0] == '-')
        {
            std::cout << ARGS_ERROR_STR << std::endl;
            return;
        }
        ++iter;
    }
    createViewFiles(parameters);
}

void create_view::createViewFiles(std::vector<std::string> &cspFileNames)
{
    for (auto const &file : cspFileNames)
    {
        std::cout << "create view:" << file << std::endl;
        if (createViewFile(file) != 0)
            exit(1);
    }
}

int create_view::createViewFile(const std::string &script_filename)
{
    std::cout << "create HttpView Class file by " << script_filename
              << std::endl;
    if (pathToNamespaceFlag_)
    {
        std::string::size_type pos1 = 0, pos2 = 0;
        if (script_filename.length() >= 2 && script_filename[0] == '.' &&
            (script_filename[1] == '/' || script_filename[1] == '\\'))
        {
            pos1 = pos2 = 2;
        }
        else if (script_filename.length() >= 1 &&
                 (script_filename[0] == '/' || script_filename[0] == '\\'))
        {
            pos1 = pos2 = 1;
        }
        while (pos2 < script_filename.length() - 1)
        {
            if (script_filename[pos2] == '/' || script_filename[pos2] == '\\')
            {
                if (pos2 > pos1)
                {
                    namespaces_.push_back(
                        script_filename.substr(pos1, pos2 - pos1));
                }
                pos1 = ++pos2;
            }
            else
            {
                ++pos2;
            }
        }
    }
    std::string npPrefix;
    for (auto &np : namespaces_)
    {
        npPrefix += np;
        npPrefix += "_";
    }
    std::ifstream infile(script_filename.c_str(), std::ifstream::in);
    if (infile)
    {
        std::string::size_type pos = script_filename.rfind('.');
        if (pos != std::string::npos)
        {
            std::string className = script_filename.substr(0, pos);
            if ((pos = className.rfind('/')) != std::string::npos)
            {
                className = className.substr(pos + 1);
            }
            std::cout << "className=" << className << std::endl;
            std::string headFileName =
                outputPath_ + "/" + npPrefix + className + ".h";
            std::string sourceFilename =
                outputPath_ + "/" + npPrefix + className + ".cc";
            std::ofstream oHeadFile(headFileName.c_str(), std::ofstream::out);
            std::ofstream oSourceFile(sourceFilename.c_str(),
                                      std::ofstream::out);
            if (!oHeadFile || !oSourceFile)
            {
                std::cerr << "Can't open " << headFileName << " or "
                          << sourceFilename << "\n";
                return -1;
            }

            newViewHeaderFile(oHeadFile, className);
            newViewSourceFile(oSourceFile, className, npPrefix, infile);
        }
        else
            return -1;
    }
    else
    {
        std::cerr << "can't open file " << script_filename << std::endl;
        return -1;
    }
    return 0;
}

void create_view::newViewHeaderFile(std::ofstream &file,
                                    const std::string &className)
{
    file << "//this file is generated by program automatically,don't modify "
            "it!\n";
    file << "#include <drogon/DrTemplate.h>\n";
    for (auto &np : namespaces_)
    {
        file << "namespace " << np << "\n";
        file << "{\n";
    }
    file << "class " << className << ":public drogon::DrTemplate<" << className
         << ">\n";
    file << "{\npublic:\n\t" << className << "(){};\n\tvirtual ~" << className
         << "(){};\n\t"
            "virtual std::string genText(const drogon::DrTemplateData &) "
            "override;\n};\n";
    for (std::size_t i = 0; i < namespaces_.size(); ++i)
    {
        file << "}\n";
    }
}

void create_view::newViewSourceFile(std::ofstream &file,
                                    const std::string &className,
                                    const std::string &namespacePrefix,
                                    std::ifstream &infile)
{
    file << "//this file is generated by program(drogon_ctl) "
            "automatically,don't modify it!\n";
    file << "#include \"" << namespacePrefix << className << ".h\"\n";
    file << "#include <drogon/utils/OStringStream.h>\n";
    file << "#include <drogon/utils/Utilities.h>\n";
    file << "#include <string>\n";
    file << "#include <map>\n";
    file << "#include <vector>\n";
    file << "#include <set>\n";
    file << "#include <iostream>\n";
    file << "#include <unordered_map>\n";
    file << "#include <unordered_set>\n";
    file << "#include <algorithm>\n";
    file << "#include <list>\n";
    file << "#include <deque>\n";
    file << "#include <queue>\n";

    // Find layout tag
    std::string layoutName;
    std::regex layoutReg("<%layout[ \\t]+(((?!%\\}).)*[^ \\t])[ \\t]*%>");
    for (std::string buffer; std::getline(infile, buffer);)
    {
        std::smatch results;
        if (std::regex_search(buffer, results, layoutReg))
        {
            if (results.size() > 1)
            {
                layoutName = results[1].str();
                break;
            }
        }
    }
    infile.clear();
    infile.seekg(0, std::ifstream::beg);
    bool import_flag{false};
    for (std::string buffer; std::getline(infile, buffer);)
    {
        std::string::size_type pos(0);

        if (!import_flag)
        {
            std::string lowerBuffer = buffer;
            std::transform(lowerBuffer.begin(),
                           lowerBuffer.end(),
                           lowerBuffer.begin(),
                           [](unsigned char c) { return tolower(c); });
            if ((pos = lowerBuffer.find(cxx_include)) != std::string::npos)
            {
                // std::cout<<"haha find it!"<<endl;
                std::string newLine = buffer.substr(pos + cxx_include.length());
                import_flag = true;
                if ((pos = newLine.find(cxx_end)) != std::string::npos)
                {
                    newLine = newLine.substr(0, pos);
                    file << newLine << "\n";
                    break;
                }
                else
                {
                    file << newLine << "\n";
                }
            }
        }
        else
        {
            // std::cout<<buffer<<endl;
            if ((pos = buffer.find(cxx_end)) != std::string::npos)
            {
                std::string newLine = buffer.substr(0, pos);
                file << newLine << "\n";
                break;
            }
            else
            {
                // std::cout<<"to source file"<<buffer<<endl;
                file << buffer << "\n";
            }
        }
    }
    // std::cout<<"import_flag="<<import_flag<<std::endl;
    if (!import_flag)
    {
        infile.clear();
        infile.seekg(0, std::ifstream::beg);
    }

    if (!namespaces_.empty())
    {
        file << "using namespace ";
        for (std::size_t i = 0; i < namespaces_.size(); ++i)
        {
            if (i != namespaces_.size() - 1)
            {
                file << namespaces_[i] << "::";
            }
            else
            {
                file << namespaces_[i] << ";";
            }
        }
        file << "\n";
    }
    file << "using namespace drogon;\n";
    std::string viewDataName = className + "_view_data";
    // virtual std::string genText(const DrTemplateData &)
    file << "std::string " << className << "::genText(const DrTemplateData& "
         << viewDataName << ")\n{\n";
    // std::string bodyName=className+"_bodystr";
    std::string streamName = className + "_tmp_stream";

    // oSrcFile <<"\tstd::string "<<bodyName<<";\n";
    file << "\tdrogon::OStringStream " << streamName << ";\n";
    file << "\tstd::string layoutName{\"" << layoutName << "\"};\n";
    int cxx_flag = 0;
    for (std::string buffer; std::getline(infile, buffer);)
    {
        if (buffer.length() > 0)
        {
            std::smatch results;
            if (std::regex_search(buffer, results, layoutReg))
            {
                if (results.size() > 1)
                {
                    continue;
                }
            }

            std::regex re("\\{%[ \\t]*(((?!%\\}).)*[^ \\t])[ \\t]*%\\}");
            buffer = std::regex_replace(buffer, re, "<%c++$$$$<<$1;%>");
        }
        parseLine(file, buffer, streamName, viewDataName, cxx_flag);
    }
    file << "if(layoutName.empty())\n{\n";
    file << "std::string ret{std::move(" << streamName << ".str())};\n";
    file << "return ret;\n}else\n{\n";
    file << "auto templ = DrTemplateBase::newTemplate(layoutName);\n";
    file << "if(!templ) return \"\";\n";
    file << "HttpViewData data = " << viewDataName << ";\n";
    file << "auto str = std::move(" << streamName << ".str());\n";
    file << "if(!str.empty() && str[str.length()-1] == '\\n') "
            "str.resize(str.length()-1);\n";
    file << "data[\"\"] = std::move(str);\n";
    file << "return templ->genText(data);\n";
    file << "}\n}\n";
}
