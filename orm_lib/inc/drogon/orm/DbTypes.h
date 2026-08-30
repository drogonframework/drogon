/**
 *
 *  @file DbTypes.h
 *  @author interfector18
 *
 *  Copyright 2020, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

namespace drogon
{
namespace orm
{
class DefaultValue
{
};

namespace internal
{
enum FieldType
{
    MySqlTiny,
    MySqlShort,
    MySqlLong,
    MySqlLongLong,
    MySqlNull,
    MySqlString,
    DrogonDefaultValue,
    MySqlUTiny,
    MySqlUShort,
    MySqlULong,
    MySqlULongLong,
};

}  // namespace internal
}  // namespace orm
}  // namespace drogon
