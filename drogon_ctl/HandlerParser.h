#pragma once
#include <drogon/HttpTypes.h>
#include <trantor/utils/Logger.h>
#include <vector>
#include <memory>
#include <string>
#include <tuple>

namespace drogon
{
namespace internal
{
class StructNode;
using StructNodePtr = std::shared_ptr<StructNode>;
class StructNode
{
  public:
    enum NodeType
    {
        kRoot = 0,
        kClass,
        kNameSpace
    };

    StructNode(const std::string &content,
               const std::string &name,
               NodeType type = kRoot)
        : type_(type), name_(name), content_(content)
    {
        LOG_DEBUG << "new node:" << name << "-" << type;
        if (type != kClass)
            children_ = parse(content);
    }
    const std::string &content() const
    {
        return content_;
    }
    const std::string &name() const
    {
        return name_;
    }
    NodeType type() const
    {
        return type_;
    }
    void print() const
    {
        print(0);
    }

  private:
    std::vector<StructNodePtr> children_;
    std::string content_;
    NodeType type_;
    std::string name_;
    std::pair<std::string, std::string> findContentOfClassOrNameSpace(
        const std::string &content,
        std::string::size_type start);
    std::pair<StructNodePtr, std::string> findClass(const std::string &content);
    std::tuple<std::string, StructNodePtr, std::string> findNameSpace(
        const std::string &content);
    std::vector<StructNodePtr> parse(const std::string &content);
    void print(int indent) const;
};

class ParametersInfo
{
  public:
    std::string getName() const;
    std::string getType() const;
    std::string getConstraint() const;
};
class RoutingInfo
{
  public:
    std::string getPath() const;
    std::vector<std::string> getFilters() const;
    std::vector<drogon::HttpMethod> getHttpMethods() const;
    std::string getScripts() const;
    std::string getContentType() const;
    std::string getReturnContentType() const;
    std::vector<ParametersInfo> getParameterInfo() const;
    std::vector<ParametersInfo> getReturnParameterInfo() const;
};

class HandlerInfo
{
  public:
    std::string getClassName() const;
    std::string getNamespace() const;
    std::vector<RoutingInfo> getRoutingInfo() const;
};
class HandlerParser
{
  public:
    std::vector<HandlerInfo> parse();
    HandlerParser(const StructNodePtr root);
};
}  // namespace internal
}  // namespace drogon