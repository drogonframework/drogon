/**
 *
 *  @file CouchBaseResultImpl.h
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
#pragma once
#include <libcouchbase/couchbase.h>
#include <libcouchbase/pktfwd.h>
#include <trantor/utils/NonCopyable.h>
#include <string>

namespace drogon
{
namespace nosql
{
class CouchBaseResultImpl : public trantor::NonCopyable
{
  public:
    virtual ~CouchBaseResultImpl()
    {
    }
    virtual const std::string& getValue() const
    {
        assert(false);
        throw std::runtime_error("Bad type");
    }

  private:
};

class GetLcbResult : public CouchBaseResultImpl
{
  public:
    GetLcbResult(const lcb_RESPGET* resp);
    virtual const std::string& getValue() const override
    {
        return value_;
    }

  private:
    std::string key_;
    std::string value_;
    uint64_t cas_{0};
    uint32_t flags_{0};
    lcb_STATUS status_{LCB_SUCCESS};
    uint8_t dataType_{0};
};

class StoreLcbResult : public CouchBaseResultImpl
{
  public:
    StoreLcbResult(const lcb_RESPSTORE* resp);

  private:
    std::string key_;
    uint64_t cas_{0};
    lcb_STATUS status_{LCB_SUCCESS};
};

}  // namespace nosql
}  // namespace drogon