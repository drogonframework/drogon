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
#include <cassert>
#include <drogon/orm/Exception.h>

using namespace drogon::orm;

Result::SizeType PostgreSQLResultImpl::size() const noexcept
{
    return result_ ? PQntuples(result_.get()) : 0;
}

Result::RowSizeType PostgreSQLResultImpl::columns() const noexcept
{
    auto ptr = const_cast<PGresult *>(result_.get());
    return ptr ? Result::RowSizeType(PQnfields(ptr)) : 0;
}

const char *PostgreSQLResultImpl::columnName(RowSizeType number) const
{
    auto ptr = const_cast<PGresult *>(result_.get());
    if (ptr)
    {
        auto N = PQfname(ptr, int(number));
        assert(N);
        return N;
    }
    throw "nullptr result";  // The program will never execute here
}

Result::SizeType PostgreSQLResultImpl::affectedRows() const noexcept
{
    char *str = PQcmdTuples(result_.get());
    if (str == nullptr || str[0] == '\0')
        return 0;
    return atol(str);
}

Result::RowSizeType PostgreSQLResultImpl::columnNumber(
    const char colName[]) const
{
    auto ptr = const_cast<PGresult *>(result_.get());
    if (ptr)
    {
        auto N = PQfnumber(ptr, colName);
        if (N == -1)
            throw RangeError(std::string("there is no column named ") +
                             colName);
        return N;
    }
    throw "nullptr result";  // The program will never execute here
}

const char *PostgreSQLResultImpl::getValue(SizeType row,
                                           RowSizeType column) const
{
    return PQgetvalue(result_.get(), int(row), int(column));
}

bool PostgreSQLResultImpl::isNull(SizeType row, RowSizeType column) const
{
    return PQgetisnull(result_.get(), int(row), int(column)) != 0;
}

Result::FieldSizeType PostgreSQLResultImpl::getLength(SizeType row,
                                                      RowSizeType column) const
{
    return PQgetlength(result_.get(), int(row), int(column));
}

int PostgreSQLResultImpl::oid(RowSizeType column) const
{
    return PQftype(result_.get(), (int)column);
}
