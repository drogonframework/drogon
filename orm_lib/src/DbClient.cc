/**
 *
 *  DbClient.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *
 */

#include <drogon/orm/DbClient.h>

using namespace drogon::orm;
using namespace drogon;

internal::SqlBinder DbClient::operator << (const std::string &sql)
{
    return internal::SqlBinder(sql,*this);
}