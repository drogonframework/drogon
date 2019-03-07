/**
 *
 *  PostgreSQLResultImpl.cc
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

#include "PostgreSQLResultImpl.h"
#include <assert.h>

using namespace drogon::orm;

Result::size_type PostgreSQLResultImpl::size() const noexcept
{
    return _result ? PQntuples(_result.get()) : 0;
}
Result::row_size_type PostgreSQLResultImpl::columns() const noexcept
{
    auto ptr = const_cast<PGresult *>(_result.get());
    return ptr ? Result::row_size_type(PQnfields(ptr)) : 0;
}
const char *PostgreSQLResultImpl::columnName(row_size_type number) const
{
    auto ptr = const_cast<PGresult *>(_result.get());
    if (ptr)
    {
        auto N = PQfname(ptr, int(number));
        assert(N);
        return N;
    }
    throw "nullptr result"; //The program will never execute here
}
Result::size_type PostgreSQLResultImpl::affectedRows() const noexcept
{
    char *str = PQcmdTuples(_result.get());
    if (str == nullptr || str[0] == '\0')
        return 0;
    return atol(str);
}
Result::row_size_type PostgreSQLResultImpl::columnNumber(const char colName[]) const
{
    auto ptr = const_cast<PGresult *>(_result.get());
    if (ptr)
    {
        auto N = PQfnumber(ptr, colName);
        if (N == -1)
            throw std::string("there is no column named ") + colName; // TODO throw detail exception here;
        return N;
    }
    throw "nullptr result"; //The program will never execute here
}
const char *PostgreSQLResultImpl::getValue(size_type row, row_size_type column) const
{
    return PQgetvalue(_result.get(), int(row), int(column));
}
bool PostgreSQLResultImpl::isNull(size_type row, row_size_type column) const
{
    return PQgetisnull(_result.get(), int(row), int(column)) != 0;
}
Result::field_size_type PostgreSQLResultImpl::getLength(size_type row, row_size_type column) const
{
    return PQgetlength(_result.get(), int(row), int(column));
}
int PostgreSQLResultImpl::oid(row_size_type column) const
{
    return PQftype(_result.get(), (int)column);
}
