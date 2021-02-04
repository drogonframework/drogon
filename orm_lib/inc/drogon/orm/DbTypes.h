/**
 *
 *  @file DbTypes.h
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

#ifndef DBTYPES_HPP
#define DBTYPES_HPP

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
};

}  // namespace internal
}  // namespace orm
}  // namespace drogon

#endif  // DBTYPES_HPP
