/**
 *
 *  Wallets.cc
 *  DO NOT EDIT. This file is generated by drogon_ctl
 *
 */

#include "Wallets.h"
#include "Users.h"
#include <drogon/utils/Utilities.h>
#include <string>

using namespace drogon;
using namespace drogon::orm;
using namespace drogon_model::postgres;

const std::string Wallets::Cols::_id = "id";
const std::string Wallets::Cols::_user_id = "user_id";
const std::string Wallets::Cols::_amount = "amount";
const std::string Wallets::primaryKeyName = "id";
const bool Wallets::hasPrimaryKey = true;
const std::string Wallets::tableName = "wallets";

const std::vector<typename Wallets::MetaData> Wallets::metaData_ = {
    {"id", "int32_t", "integer", 4, 1, 1, 1},
    {"user_id", "std::string", "character varying", 32, 0, 0, 0},
    {"amount", "std::string", "numeric", 0, 0, 0, 0}};

const std::string &Wallets::getColumnName(size_t index) noexcept(false)
{
    assert(index < metaData_.size());
    return metaData_[index].colName_;
}

Wallets::Wallets(const Row &r, const ssize_t indexOffset) noexcept
{
    if (indexOffset < 0)
    {
        if (!r["id"].isNull())
        {
            id_ = std::make_shared<int32_t>(r["id"].as<int32_t>());
        }
        if (!r["user_id"].isNull())
        {
            userId_ =
                std::make_shared<std::string>(r["user_id"].as<std::string>());
        }
        if (!r["amount"].isNull())
        {
            amount_ =
                std::make_shared<std::string>(r["amount"].as<std::string>());
        }
    }
    else
    {
        size_t offset = (size_t)indexOffset;
        if (offset + 3 > r.size())
        {
            LOG_FATAL << "Invalid SQL result for this model";
            return;
        }
        size_t index;
        index = offset + 0;
        if (!r[index].isNull())
        {
            id_ = std::make_shared<int32_t>(r[index].as<int32_t>());
        }
        index = offset + 1;
        if (!r[index].isNull())
        {
            userId_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 2;
        if (!r[index].isNull())
        {
            amount_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
    }
}

Wallets::Wallets(
    const Json::Value &pJson,
    const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if (pMasqueradingVector.size() != 3)
    {
        LOG_ERROR << "Bad masquerading vector";
        return;
    }
    if (!pMasqueradingVector[0].empty() &&
        pJson.isMember(pMasqueradingVector[0]))
    {
        dirtyFlag_[0] = true;
        if (!pJson[pMasqueradingVector[0]].isNull())
        {
            id_ = std::make_shared<int32_t>(
                (int32_t)pJson[pMasqueradingVector[0]].asInt64());
        }
    }
    if (!pMasqueradingVector[1].empty() &&
        pJson.isMember(pMasqueradingVector[1]))
    {
        dirtyFlag_[1] = true;
        if (!pJson[pMasqueradingVector[1]].isNull())
        {
            userId_ = std::make_shared<std::string>(
                pJson[pMasqueradingVector[1]].asString());
        }
    }
    if (!pMasqueradingVector[2].empty() &&
        pJson.isMember(pMasqueradingVector[2]))
    {
        dirtyFlag_[2] = true;
        if (!pJson[pMasqueradingVector[2]].isNull())
        {
            amount_ = std::make_shared<std::string>(
                pJson[pMasqueradingVector[2]].asString());
        }
    }
}

Wallets::Wallets(const Json::Value &pJson) noexcept(false)
{
    if (pJson.isMember("id"))
    {
        dirtyFlag_[0] = true;
        if (!pJson["id"].isNull())
        {
            id_ = std::make_shared<int32_t>((int32_t)pJson["id"].asInt64());
        }
    }
    if (pJson.isMember("user_id"))
    {
        dirtyFlag_[1] = true;
        if (!pJson["user_id"].isNull())
        {
            userId_ =
                std::make_shared<std::string>(pJson["user_id"].asString());
        }
    }
    if (pJson.isMember("amount"))
    {
        dirtyFlag_[2] = true;
        if (!pJson["amount"].isNull())
        {
            amount_ = std::make_shared<std::string>(pJson["amount"].asString());
        }
    }
}

void Wallets::updateByMasqueradedJson(
    const Json::Value &pJson,
    const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if (pMasqueradingVector.size() != 3)
    {
        LOG_ERROR << "Bad masquerading vector";
        return;
    }
    if (!pMasqueradingVector[0].empty() &&
        pJson.isMember(pMasqueradingVector[0]))
    {
        if (!pJson[pMasqueradingVector[0]].isNull())
        {
            id_ = std::make_shared<int32_t>(
                (int32_t)pJson[pMasqueradingVector[0]].asInt64());
        }
    }
    if (!pMasqueradingVector[1].empty() &&
        pJson.isMember(pMasqueradingVector[1]))
    {
        dirtyFlag_[1] = true;
        if (!pJson[pMasqueradingVector[1]].isNull())
        {
            userId_ = std::make_shared<std::string>(
                pJson[pMasqueradingVector[1]].asString());
        }
    }
    if (!pMasqueradingVector[2].empty() &&
        pJson.isMember(pMasqueradingVector[2]))
    {
        dirtyFlag_[2] = true;
        if (!pJson[pMasqueradingVector[2]].isNull())
        {
            amount_ = std::make_shared<std::string>(
                pJson[pMasqueradingVector[2]].asString());
        }
    }
}

void Wallets::updateByJson(const Json::Value &pJson) noexcept(false)
{
    if (pJson.isMember("id"))
    {
        if (!pJson["id"].isNull())
        {
            id_ = std::make_shared<int32_t>((int32_t)pJson["id"].asInt64());
        }
    }
    if (pJson.isMember("user_id"))
    {
        dirtyFlag_[1] = true;
        if (!pJson["user_id"].isNull())
        {
            userId_ =
                std::make_shared<std::string>(pJson["user_id"].asString());
        }
    }
    if (pJson.isMember("amount"))
    {
        dirtyFlag_[2] = true;
        if (!pJson["amount"].isNull())
        {
            amount_ = std::make_shared<std::string>(pJson["amount"].asString());
        }
    }
}

const int32_t &Wallets::getValueOfId() const noexcept
{
    static const int32_t defaultValue = int32_t();
    if (id_)
        return *id_;
    return defaultValue;
}

const std::shared_ptr<int32_t> &Wallets::getId() const noexcept
{
    return id_;
}

void Wallets::setId(const int32_t &pId) noexcept
{
    id_ = std::make_shared<int32_t>(pId);
    dirtyFlag_[0] = true;
}

const typename Wallets::PrimaryKeyType &Wallets::getPrimaryKey() const
{
    assert(id_);
    return *id_;
}

const std::string &Wallets::getValueOfUserId() const noexcept
{
    static const std::string defaultValue = std::string();
    if (userId_)
        return *userId_;
    return defaultValue;
}

const std::shared_ptr<std::string> &Wallets::getUserId() const noexcept
{
    return userId_;
}

void Wallets::setUserId(const std::string &pUserId) noexcept
{
    userId_ = std::make_shared<std::string>(pUserId);
    dirtyFlag_[1] = true;
}

void Wallets::setUserId(std::string &&pUserId) noexcept
{
    userId_ = std::make_shared<std::string>(std::move(pUserId));
    dirtyFlag_[1] = true;
}

void Wallets::setUserIdToNull() noexcept
{
    userId_.reset();
    dirtyFlag_[1] = true;
}

const std::string &Wallets::getValueOfAmount() const noexcept
{
    static const std::string defaultValue = std::string();
    if (amount_)
        return *amount_;
    return defaultValue;
}

const std::shared_ptr<std::string> &Wallets::getAmount() const noexcept
{
    return amount_;
}

void Wallets::setAmount(const std::string &pAmount) noexcept
{
    amount_ = std::make_shared<std::string>(pAmount);
    dirtyFlag_[2] = true;
}

void Wallets::setAmount(std::string &&pAmount) noexcept
{
    amount_ = std::make_shared<std::string>(std::move(pAmount));
    dirtyFlag_[2] = true;
}

void Wallets::setAmountToNull() noexcept
{
    amount_.reset();
    dirtyFlag_[2] = true;
}

void Wallets::updateId(const uint64_t id)
{
}

const std::vector<std::string> &Wallets::insertColumns() noexcept
{
    static const std::vector<std::string> inCols = {"user_id", "amount"};
    return inCols;
}

void Wallets::outputArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if (dirtyFlag_[1])
    {
        if (getUserId())
        {
            binder << getValueOfUserId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[2])
    {
        if (getAmount())
        {
            binder << getValueOfAmount();
        }
        else
        {
            binder << nullptr;
        }
    }
}

const std::vector<std::string> Wallets::updateColumns() const
{
    std::vector<std::string> ret;
    if (dirtyFlag_[1])
    {
        ret.push_back(getColumnName(1));
    }
    if (dirtyFlag_[2])
    {
        ret.push_back(getColumnName(2));
    }
    return ret;
}

void Wallets::updateArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if (dirtyFlag_[1])
    {
        if (getUserId())
        {
            binder << getValueOfUserId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[2])
    {
        if (getAmount())
        {
            binder << getValueOfAmount();
        }
        else
        {
            binder << nullptr;
        }
    }
}

Json::Value Wallets::toJson() const
{
    Json::Value ret;
    if (getId())
    {
        ret["id"] = getValueOfId();
    }
    else
    {
        ret["id"] = Json::Value();
    }
    if (getUserId())
    {
        ret["user_id"] = getValueOfUserId();
    }
    else
    {
        ret["user_id"] = Json::Value();
    }
    if (getAmount())
    {
        ret["amount"] = getValueOfAmount();
    }
    else
    {
        ret["amount"] = Json::Value();
    }
    return ret;
}

Json::Value Wallets::toMasqueradedJson(
    const std::vector<std::string> &pMasqueradingVector) const
{
    Json::Value ret;
    if (pMasqueradingVector.size() == 3)
    {
        if (!pMasqueradingVector[0].empty())
        {
            if (getId())
            {
                ret[pMasqueradingVector[0]] = getValueOfId();
            }
            else
            {
                ret[pMasqueradingVector[0]] = Json::Value();
            }
        }
        if (!pMasqueradingVector[1].empty())
        {
            if (getUserId())
            {
                ret[pMasqueradingVector[1]] = getValueOfUserId();
            }
            else
            {
                ret[pMasqueradingVector[1]] = Json::Value();
            }
        }
        if (!pMasqueradingVector[2].empty())
        {
            if (getAmount())
            {
                ret[pMasqueradingVector[2]] = getValueOfAmount();
            }
            else
            {
                ret[pMasqueradingVector[2]] = Json::Value();
            }
        }
        return ret;
    }
    LOG_ERROR << "Masquerade failed";
    if (getId())
    {
        ret["id"] = getValueOfId();
    }
    else
    {
        ret["id"] = Json::Value();
    }
    if (getUserId())
    {
        ret["user_id"] = getValueOfUserId();
    }
    else
    {
        ret["user_id"] = Json::Value();
    }
    if (getAmount())
    {
        ret["amount"] = getValueOfAmount();
    }
    else
    {
        ret["amount"] = Json::Value();
    }
    return ret;
}

bool Wallets::validateJsonForCreation(const Json::Value &pJson,
                                      std::string &err)
{
    if (pJson.isMember("id"))
    {
        if (!validJsonOfField(0, "id", pJson["id"], err, true))
            return false;
    }
    if (pJson.isMember("user_id"))
    {
        if (!validJsonOfField(1, "user_id", pJson["user_id"], err, true))
            return false;
    }
    if (pJson.isMember("amount"))
    {
        if (!validJsonOfField(2, "amount", pJson["amount"], err, true))
            return false;
    }
    return true;
}

bool Wallets::validateMasqueradedJsonForCreation(
    const Json::Value &pJson,
    const std::vector<std::string> &pMasqueradingVector,
    std::string &err)
{
    if (pMasqueradingVector.size() != 3)
    {
        err = "Bad masquerading vector";
        return false;
    }
    try
    {
        if (!pMasqueradingVector[0].empty())
        {
            if (pJson.isMember(pMasqueradingVector[0]))
            {
                if (!validJsonOfField(0,
                                      pMasqueradingVector[0],
                                      pJson[pMasqueradingVector[0]],
                                      err,
                                      true))
                    return false;
            }
        }
        if (!pMasqueradingVector[1].empty())
        {
            if (pJson.isMember(pMasqueradingVector[1]))
            {
                if (!validJsonOfField(1,
                                      pMasqueradingVector[1],
                                      pJson[pMasqueradingVector[1]],
                                      err,
                                      true))
                    return false;
            }
        }
        if (!pMasqueradingVector[2].empty())
        {
            if (pJson.isMember(pMasqueradingVector[2]))
            {
                if (!validJsonOfField(2,
                                      pMasqueradingVector[2],
                                      pJson[pMasqueradingVector[2]],
                                      err,
                                      true))
                    return false;
            }
        }
    }
    catch (const Json::LogicError &e)
    {
        err = e.what();
        return false;
    }
    return true;
}

bool Wallets::validateJsonForUpdate(const Json::Value &pJson, std::string &err)
{
    if (pJson.isMember("id"))
    {
        if (!validJsonOfField(0, "id", pJson["id"], err, false))
            return false;
    }
    else
    {
        err =
            "The value of primary key must be set in the json object for "
            "update";
        return false;
    }
    if (pJson.isMember("user_id"))
    {
        if (!validJsonOfField(1, "user_id", pJson["user_id"], err, false))
            return false;
    }
    if (pJson.isMember("amount"))
    {
        if (!validJsonOfField(2, "amount", pJson["amount"], err, false))
            return false;
    }
    return true;
}

bool Wallets::validateMasqueradedJsonForUpdate(
    const Json::Value &pJson,
    const std::vector<std::string> &pMasqueradingVector,
    std::string &err)
{
    if (pMasqueradingVector.size() != 3)
    {
        err = "Bad masquerading vector";
        return false;
    }
    try
    {
        if (!pMasqueradingVector[0].empty() &&
            pJson.isMember(pMasqueradingVector[0]))
        {
            if (!validJsonOfField(0,
                                  pMasqueradingVector[0],
                                  pJson[pMasqueradingVector[0]],
                                  err,
                                  false))
                return false;
        }
        else
        {
            err =
                "The value of primary key must be set in the json object for "
                "update";
            return false;
        }
        if (!pMasqueradingVector[1].empty() &&
            pJson.isMember(pMasqueradingVector[1]))
        {
            if (!validJsonOfField(1,
                                  pMasqueradingVector[1],
                                  pJson[pMasqueradingVector[1]],
                                  err,
                                  false))
                return false;
        }
        if (!pMasqueradingVector[2].empty() &&
            pJson.isMember(pMasqueradingVector[2]))
        {
            if (!validJsonOfField(2,
                                  pMasqueradingVector[2],
                                  pJson[pMasqueradingVector[2]],
                                  err,
                                  false))
                return false;
        }
    }
    catch (const Json::LogicError &e)
    {
        err = e.what();
        return false;
    }
    return true;
}

bool Wallets::validJsonOfField(size_t index,
                               const std::string &fieldName,
                               const Json::Value &pJson,
                               std::string &err,
                               bool isForCreation)
{
    switch (index)
    {
        case 0:
            if (pJson.isNull())
            {
                err = "The " + fieldName + " column cannot be null";
                return false;
            }
            if (isForCreation)
            {
                err = "The automatic primary key cannot be set";
                return false;
            }
            if (!pJson.isInt())
            {
                err = "Type error in the " + fieldName + " field";
                return false;
            }
            break;
        case 1:
            if (pJson.isNull())
            {
                return true;
            }
            if (!pJson.isString())
            {
                err = "Type error in the " + fieldName + " field";
                return false;
            }
            // asString().length() creates a string object, is there any better
            // way to validate the length?
            if (pJson.isString() && pJson.asString().length() > 32)
            {
                err = "String length exceeds limit for the " + fieldName +
                      " field (the maximum value is 32)";
                return false;
            }

            break;
        case 2:
            if (pJson.isNull())
            {
                return true;
            }
            if (!pJson.isString())
            {
                err = "Type error in the " + fieldName + " field";
                return false;
            }
            break;
        default:
            err = "Internal error in the server";
            return false;
    }
    return true;
}

Users Wallets::getUser(const DbClientPtr &clientPtr) const
{
    static const std::string sql = "select * from users where user_id = $1";
    Result r(nullptr);
    {
        auto binder = *clientPtr << sql;
        binder << *userId_ << Mode::Blocking >>
            [&r](const Result &result) { r = result; };
        binder.exec();
    }
    if (r.size() == 0)
    {
        throw UnexpectedRows("0 rows found");
    }
    else if (r.size() > 1)
    {
        throw UnexpectedRows("Found more than one row");
    }
    return Users(r[0]);
}

void Wallets::getUser(const DbClientPtr &clientPtr,
                      const std::function<void(Users)> &rcb,
                      const ExceptionCallback &ecb) const
{
    static const std::string sql = "select * from users where user_id = $1";
    *clientPtr << sql << *userId_ >> [rcb = std::move(rcb),
                                      ecb](const Result &r) {
        if (r.size() == 0)
        {
            ecb(UnexpectedRows("0 rows found"));
        }
        else if (r.size() > 1)
        {
            ecb(UnexpectedRows("Found more than one row"));
        }
        else
        {
            rcb(Users(r[0]));
        }
    } >> ecb;
}
