/**
 *
 *  PgClient.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Definitions for the PostgreSQL client class
 *
 */
#pragma once

#include <drogon/orm/DbClient.h>
#include <memory>

namespace drogon
{
namespace orm
{
class PgClientImpl;
class PgClient : public DbClient
{
  public:
    PgClient(const std::string &connInfo, const size_t connNum);
    virtual std::string replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const override;

  private:
    virtual void execSql(const std::string &sql,
                         size_t paraNum,
                         const std::vector<const char *> &parameters,
                         const std::vector<int> &length,
                         const std::vector<int> &format,
                         const ResultCallback &rcb,
                         const std::function<void(const std::exception_ptr &)> &exptCallback) override;
    std::shared_ptr<PgClientImpl> _clientPtr;
};

} // namespace orm
} // namespace drogon