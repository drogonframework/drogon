#include "HandlerParser.h"
#include <iostream>
#include <regex>

using namespace drogon::internal;

std::pair<std::string, std::string> StructNode::findContentOfClassOrNameSpace(
    const std::string &content,
    std::string::size_type start)
{
    int braces = 0;
    std::string::size_type pos1{start};
    std::string::size_type pos2{content.size() - 1};
    for (auto i = start; i < content.size(); i++)
    {
        if (content[i] == '{')
        {
            braces++;
            if (braces == 1)
            {
                pos1 = i + 1;
            }
        }
        else if (content[i] == '}')
        {
            braces--;
            if (braces == 0)
            {
                pos2 = i;
                break;
            }
        }
    }
    return std::pair<std::string, std::string>(content.substr(pos1,
                                                              pos2 - pos1),
                                               content.substr(pos2 + 1));
}
std::pair<StructNodePtr, std::string> StructNode::findClass(
    const std::string &content)
{
    LOG_DEBUG << "findClass: " << content;
    if (content.empty())
        return std::pair<StructNodePtr, std::string>(nullptr, "");
    std::regex rx(R"(class[ \r\n]+([^ \r\n\{]+)[ \r\n\{:]+)");
    std::smatch results;
    if (std::regex_search(content, results, rx))
    {
        assert(results.size() > 1);
        auto nextPart =
            findContentOfClassOrNameSpace(content, results.position());
        return std::pair<StructNodePtr, std::string>(
            std::make_shared<StructNode>(nextPart.first,
                                         results[1].str(),
                                         kClass),
            nextPart.second);
    }
    return std::pair<StructNodePtr, std::string>(nullptr, "");
}
std::tuple<std::string, StructNodePtr, std::string> StructNode::findNameSpace(
    const std::string &content)
{
    LOG_DEBUG << "findNameSpace";
    if (content.empty())
        return std::tuple<std::string, StructNodePtr, std::string>("",
                                                                   nullptr,
                                                                   "");
    std::regex rx(R"(namespace[ \r\n]+([^ \r\n]+)[ \r\n]*\{)");
    std::smatch results;
    if (std::regex_search(content, results, rx))
    {
        assert(results.size() > 1);
        auto pos = results.position();
        auto first = content.substr(0, pos);
        auto nextPart = findContentOfClassOrNameSpace(content, pos);
        auto npNodePtr = std::make_shared<StructNode>(nextPart.first,
                                                      results[1].str(),
                                                      kNameSpace);

        return std::tuple<std::string, StructNodePtr, std::string>(
            first, npNodePtr, nextPart.second);
    }
    else
    {
        return std::tuple<std::string, StructNodePtr, std::string>("",
                                                                   nullptr,
                                                                   "");
    }
}
std::vector<StructNodePtr> StructNode::parse(const std::string &content)
{
    std::vector<StructNodePtr> res;
    auto t = findNameSpace(content);
    if (std::get<1>(t))
    {
        res.emplace_back(std::get<1>(t));
        auto firstPart = std::get<0>(t);
        while (1)
        {
            auto p = findClass(firstPart);
            if (p.first)
            {
                res.emplace_back(p.first);
                firstPart = p.second;
            }
            else
            {
                break;
            }
        }
        auto subsequentNode = parse(std::get<2>(t));
        for (auto &node : subsequentNode)
        {
            res.emplace_back(node);
        }
        return res;
    }
    std::string classPart = content;
    while (1)
    {
        auto p = findClass(classPart);
        if (p.first)
        {
            res.emplace_back(p.first);
            classPart = p.second;
        }
        else
        {
            break;
        }
    }
    return res;
}
void StructNode::print(int indent) const
{
    std::string ind(indent, ' ');
    std::cout << ind;
    switch (type_)
    {
        case kRoot:
        {
            std::cout << "Root\n" << ind << "{\n";
            break;
        }
        case kClass:
        {
            std::cout << "class " << name_ << "\n" << ind << "{\n";
            break;
        }
        case kNameSpace:
        {
            std::cout << "namespace " << name_ << "\n" << ind << "{\n";
            break;
        }
    }

    for (auto child : children_)
    {
        child->print(indent + 2);
    }
    if (type_ == kClass)
    {
        std::cout << content_ << "\n";
    }
    std::cout << ind << "}";
    if (type_ == kClass)
        std::cout << ";";
    std::cout << "\n";
}