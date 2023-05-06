#include "YamlConfigAdapter.h"
#ifdef HAS_YAML_CPP
#include <yaml-cpp/yaml.h>
#endif
using namespace drogon;
#ifdef HAS_YAML_CPP
namespace YAML
{
static bool yaml2json(const Node &node, Json::Value &jsonValue)
{
    if (node.IsNull())
    {
        return false;
    }
    else if (node.IsScalar())
    {
        if (node.Tag() != "!")
        {
            try
            {
                jsonValue = node.as<Json::Value::Int64>();
                return true;
            }
            catch (const YAML::BadConversion &e)
            {
            }
            try
            {
                jsonValue = node.as<double>();
                return true;
            }
            catch (const YAML::BadConversion &e)
            {
            }
            try
            {
                jsonValue = node.as<bool>();
                return true;
            }
            catch (const YAML::BadConversion &e)
            {
            }
        }

        Json::Value v(node.Scalar());
        jsonValue.swapPayload(v);
        return true;
    }
    else if (node.IsSequence())
    {
        for (std::size_t i = 0; i < node.size(); i++)
        {
            Json::Value v;
            if (yaml2json(node[i], v))
            {
                jsonValue.append(v);
            }
            else
            {
                return false;
            }
        }

        return true;
    }
    else if (node.IsMap())
    {
        for (YAML::const_iterator it = node.begin(); it != node.end(); ++it)
        {
            Json::Value v;
            if (yaml2json(it->second, v))
            {
                jsonValue[it->first.Scalar()] = v;
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    return false;
}

template <>
struct convert<Json::Value>
{
    static bool decode(const Node &node, Json::Value &rhs)
    {
        return yaml2json(node, rhs);
    };
};
}  // namespace YAML

#endif
Json::Value YamlConfigAdapter::getJson(const std::string &content) const
    noexcept(false)
{
#if HAS_YAML_CPP
    // parse yaml file
    YAML::Node config = YAML::Load(content);
    if (!config.IsNull())
    {
        return config.as<Json::Value>();
    }
    else
        return Json::Value();
#else
    throw std::runtime_error("please install yaml-cpp library");
#endif
}

std::vector<std::string> YamlConfigAdapter::getExtensions() const
{
    return {"yaml", "yml"};
}
