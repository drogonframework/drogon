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
#include <algorithm>
#include <cctype>
#include <vector>

namespace drogon
{
namespace orm
{
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
    auto op = json[1].asString();
    auto first = std::find_if_not(op.begin(), op.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    auto last = std::find_if_not(op.rbegin(), op.rend(), [](unsigned char c) {
                    return std::isspace(c);
                }).base();
    op = first < last ? std::string(first, last) : std::string{};
    std::transform(op.begin(), op.end(), op.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const auto column = json[0].asString();
    if (json[2].isNull())
    {
        if (op == "=")
        {
            *this = Criteria(column, CompareOperator::IsNull);
        }
        else if (op == "!=" || op == "<>")
        {
            *this = Criteria(column, CompareOperator::IsNotNull);
        }
        else
        {
            throw std::runtime_error("Json format error");
        }
    }
    else if (json[2].isArray())
    {
        if (op != "in" || json[2].empty())
        {
            throw std::runtime_error("Json format error");
        }
        std::vector<std::string> values;
        values.reserve(json[2].size());
        for (const auto &value : json[2])
        {
            values.push_back(value.asString());
        }
        *this = Criteria(column, CompareOperator::In, std::move(values));
    }
    else
    {
        CompareOperator compare;
        if (op == "=")
            compare = CompareOperator::EQ;
        else if (op == "!=" || op == "<>")
            compare = CompareOperator::NE;
        else if (op == "<")
            compare = CompareOperator::LT;
        else if (op == "<=")
            compare = CompareOperator::LE;
        else if (op == ">")
            compare = CompareOperator::GT;
        else if (op == ">=")
            compare = CompareOperator::GE;
        else if (op == "like")
            compare = CompareOperator::Like;
        else if (op == "not like")
            compare = CompareOperator::NotLike;
        else
            throw std::runtime_error("Json format error");
        *this = Criteria(column, compare, json[2].asString());
    }
}

}  // namespace orm
}  // namespace drogon
