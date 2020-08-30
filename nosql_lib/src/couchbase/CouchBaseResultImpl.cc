/**
 *
 *  @file CouchBaseResultImpl.cc
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
#include <drogon/nosql/CouchBaseResult.h>
#include "CouchBaseResultImpl.h"
using namespace drogon::nosql;

GetLcbResult::GetLcbResult(const lcb_RESPGET* resp)
{
    status_ = lcb_respget_status(resp);
    if (status_ != LCB_SUCCESS)
        return;
    const char* key{nullptr};
    size_t key_len{0};
    lcb_respget_key(resp, &key, &key_len);
    key_ = std::string(key, key_len);
    const char* value{nullptr};
    size_t value_len{0};
    lcb_respget_value(resp, &value, &value_len);
    value_ = std::string(value, value_len);
    lcb_respget_cas(resp, &cas_);
    lcb_respget_flags(resp, &flags_);
    lcb_respget_datatype(resp, &dataType_);
}

StoreLcbResult::StoreLcbResult(const lcb_RESPSTORE* resp)
{
}

const std::string& CouchBaseResult::getValue() const
{
    return resultPtr_->getValue();
}