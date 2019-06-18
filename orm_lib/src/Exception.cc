/**
 *
 * Copyright (c) 2005-2017, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 */
// taken from libpqxx and modified

/**
 *
 *  Exception.cc
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

#include <drogon/orm/Exception.h>

using namespace drogon::orm;

DrogonDbException::~DrogonDbException() noexcept
{
}

Failure::Failure(const std::string &whatarg) : std::runtime_error(whatarg)
{
}

BrokenConnection::BrokenConnection() : Failure("Connection to database failed")
{
}

BrokenConnection::BrokenConnection(const std::string &whatarg)
    : Failure(whatarg)
{
}

SqlError::SqlError(const std::string &whatarg,
                   const std::string &Q,
                   const char sqlstate[])
    : Failure(whatarg), _query(Q), _sqlState(sqlstate ? sqlstate : "")
{
}

SqlError::~SqlError() noexcept
{
}

const std::string &SqlError::query() const noexcept
{
    return _query;
}

const std::string &SqlError::sqlState() const noexcept
{
    return _sqlState;
}

InDoubtError::InDoubtError(const std::string &whatarg) : Failure(whatarg)
{
}

TransactionRollback::TransactionRollback(const std::string &whatarg)
    : Failure(whatarg)
{
}

SerializationFailure::SerializationFailure(const std::string &whatarg)
    : TransactionRollback(whatarg)
{
}

StatementCompletionUnknown::StatementCompletionUnknown(
    const std::string &whatarg)
    : TransactionRollback(whatarg)
{
}

DeadlockDetected::DeadlockDetected(const std::string &whatarg)
    : TransactionRollback(whatarg)
{
}

InternalError::InternalError(const std::string &whatarg)
    : logic_error("libpqxx internal error: " + whatarg)
{
}

UsageError::UsageError(const std::string &whatarg) : logic_error(whatarg)
{
}

ArgumentError::ArgumentError(const std::string &whatarg)
    : invalid_argument(whatarg)
{
}

ConversionError::ConversionError(const std::string &whatarg)
    : domain_error(whatarg)
{
}

RangeError::RangeError(const std::string &whatarg) : out_of_range(whatarg)
{
}
