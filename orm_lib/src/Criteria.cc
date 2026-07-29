/**
 *
 *  @file Criteria.cc
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

#include <drogon/orm/Criteria.h>
#include <json/json.h>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace drogon
{
namespace orm
{
namespace
{
// Normalize an operator token: trim surrounding whitespace, lowercase, and
// collapse internal runs of whitespace to a single space, so that both "> ="
// style spacing and case variations map to a single canonical form.
std::string normalizeOperator(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size());
    bool pendingSpace = false;
    for (char c : raw)
    {
        auto uc = static_cast<unsigned char>(c);
        if (std::isspace(uc))
        {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !out.empty())
            out.push_back(' ');
        pendingSpace = false;
        out.push_back(static_cast<char>(std::tolower(uc)));
    }
    return out;
}

// Map a caller-supplied comparison operator to a fixed, safe SQL fragment.
// In the JSON form of Criteria the operator is concatenated straight into the
// query string (it cannot be bound as a parameter), so it must be restricted
// to a known set to avoid SQL injection through an attacker-controlled filter.
const std::string &comparisonOperatorSql(const std::string &raw)
{
    static const std::unordered_map<std::string, std::string> allowed = {
        {"=", " = "},
        {"!=", " != "},
        {"<>", " <> "},
        {">", " > "},
        {">=", " >= "},
        {"<", " < "},
        {"<=", " <= "},
        {"like", " like "},
        {"not like", " not like "},
        {"ilike", " ilike "},
        {"not ilike", " not ilike "},
    };
    auto it = allowed.find(normalizeOperator(raw));
    if (it == allowed.end())
    {
        throw std::runtime_error("Invalid comparison operator in Criteria");
    }
    return it->second;
}
}  // namespace

const Criteria operator&&(Criteria cond1, Criteria cond2)
{
    bool cond1valid = (bool)cond1, cond2valid = (bool)cond2;
    assert(cond1valid || cond2valid);
    if (cond1valid && !cond2valid)
    {
        return cond1;
    }
    if (!cond1valid && cond2valid)
    {
        return cond2;
    }
    Criteria cond;
    cond.conditionString_ = "( ";
    cond.conditionString_ += cond1.conditionString_;
    cond.conditionString_ += " ) and ( ";
    cond.conditionString_ += cond2.conditionString_;
    cond.conditionString_ += " )";
    auto cond1Ptr = std::make_shared<Criteria>(std::move(cond1));
    auto cond2Ptr = std::make_shared<Criteria>(std::move(cond2));
    cond.outputArgumentsFunc_ = [cond1Ptr,
                                 cond2Ptr](internal::SqlBinder &binder) {
        if (cond1Ptr->outputArgumentsFunc_)
        {
            cond1Ptr->outputArgumentsFunc_(binder);
        }
        if (cond2Ptr->outputArgumentsFunc_)
        {
            cond2Ptr->outputArgumentsFunc_(binder);
        }
    };
    return cond;
}

const Criteria operator||(Criteria cond1, Criteria cond2)
{
    bool cond1valid = (bool)cond1, cond2valid = (bool)cond2;
    assert(cond1valid || cond2valid);
    if (cond1valid && !cond2valid)
    {
        return cond1;
    }
    if (!cond1valid && cond2valid)
    {
        return cond2;
    }
    Criteria cond;
    cond.conditionString_ = "( ";
    cond.conditionString_ += cond1.conditionString_;
    cond.conditionString_ += " ) or ( ";
    cond.conditionString_ += cond2.conditionString_;
    cond.conditionString_ += " )";
    auto cond1Ptr = std::make_shared<Criteria>(std::move(cond1));
    auto cond2Ptr = std::make_shared<Criteria>(std::move(cond2));
    cond.outputArgumentsFunc_ = [cond1Ptr,
                                 cond2Ptr](internal::SqlBinder &binder) {
        if (cond1Ptr->outputArgumentsFunc_)
        {
            cond1Ptr->outputArgumentsFunc_(binder);
        }
        if (cond2Ptr->outputArgumentsFunc_)
        {
            cond2Ptr->outputArgumentsFunc_(binder);
        }
    };
    return cond;
}

Criteria::Criteria(const Json::Value &json) noexcept(false)
{
    if (!json.isArray() || json.size() != 3)
    {
        throw std::runtime_error("Json format error");
    }
    if (!json[0].isString() || !json[1].isString())
    {
        throw std::runtime_error("Json format error");
    }
    conditionString_ = json[0].asString();
    if (!json[2].isNull() && !json[2].isArray())
    {
        if (json[1].asString() == "in")
        {
            throw std::runtime_error("Json format error");
        }
        conditionString_.append(comparisonOperatorSql(json[1].asString()));
        conditionString_.append("$?");
        outputArgumentsFunc_ =
            [arg = json[2].asString()](internal::SqlBinder &binder) {
                binder << arg;
            };
    }
    else if (json[2].isNull())
    {
        if (json[1].asString() == "=")
        {
            conditionString_.append(" is null");
        }
        else if (json[1].asString() == "!=")
        {
            conditionString_.append(" is not null");
        }
        else
        {
            throw std::runtime_error("Json format error");
        }
    }
    else
    {
        assert(json[2].isArray());
        if (json[1].asString() != "in")
        {
            throw std::runtime_error("Json format error");
        }
        conditionString_.append(" in (");
        for (size_t i = 0; i < json[2].size(); ++i)
        {
            if (i < json[2].size() - 1)
            {
                conditionString_.append("$?,");
            }
            else
            {
                conditionString_.append("$?");
            }
        }
        conditionString_.append(1, ')');
        outputArgumentsFunc_ = [args = json[2]](internal::SqlBinder &binder) {
            for (auto &arg : args)
            {
                binder << arg.asString();
            }
        };
    }
}

}  // namespace orm
}  // namespace drogon
