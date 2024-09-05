/** Definition of Drogon Db exception classes.
 *
 * Copyright (c) 2003-2018, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 */

// taken from libpqxx and modified

/**
 *
 *  @file Exception.h
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

#pragma once

#include <drogon/exports.h>
#include <functional>
#include <stdexcept>
#include <string>

namespace drogon
{
namespace orm
{
/// Mixin base class to identify drogon-db-specific exception types
/**
 * If you wish to catch all exception types specific to drogon db for some
 * reason,
 * catch this type.  All of drogon db's exception classes are derived from it
 * through multiple-inheritance (they also fit into the standard library's
 * exception hierarchy in more fitting places).
 *
 * This class is not derived from std::exception, since that could easily lead
 * to exception classes with multiple std::exception base-class objects.  As
 * Bart Samwel points out, "catch" is subject to some nasty fineprint in such
 * cases.
 */
class DROGON_EXPORT DrogonDbException
{
  public:
    /// Support run-time polymorphism, and keep this class abstract
    virtual ~DrogonDbException() noexcept
    {
    }

    /// Return std::exception base-class object
    /**
     * Use this to get at the exception's what() function, or to downcast to a
     * more specific type using dynamic_cast.
     *
     * Casting directly from DrogonDbException to a specific exception type is
     * not likely to work since DrogonDbException is not (and could not safely
     * be) derived from std::exception.
     *
     * For example, to test dynamically whether an exception is an SqlError:
     *
     * @code
     * try
     * {
     *   // ...
     * }
     * catch (const drogon::orm::DrogonDbException &e)
     * {
     *   std::cerr << e.base().what() << std::endl;
     *   const drogon::orm::SqlError *s=dynamic_cast<const
     * drogon::orm::SqlError*>(&e.base());
     *   if (s) std::cerr << "Query was: " << s->query() << std::endl;
     * }
     * @endcode
     */
    virtual const std::exception &base() const noexcept
    {
        static std::exception except;
        return except;
    }  //[t00]
};

/// Run-time Failure encountered by drogon orm lib, similar to
/// std::runtime_error
class Failure : public DrogonDbException, public std::runtime_error
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    DROGON_EXPORT explicit Failure(const std::string &);
};

/// Exception class for lost or failed backend connection.
/**
 * @warning When this happens on Unix-like systems, you may also get a SIGPIPE
 * signal.  That signal aborts the program by default, so if you wish to be able
 * to continue after a connection breaks, be sure to disarm this signal.
 *
 * If you're working on a Unix-like system, see the manual page for
 * @c signal (2) on how to deal with SIGPIPE.  The easiest way to make this
 * signal harmless is to make your program ignore it:
 *
 * @code
 * #include <signal.h>
 *
 * int main()
 * {
 *   signal(SIGPIPE, SIG_IGN);
 *   // ...
 * @endcode
 */
class BrokenConnection : public Failure
{
  public:
    DROGON_EXPORT BrokenConnection();
    DROGON_EXPORT explicit BrokenConnection(const std::string &);
};

/// Exception class for failed queries.
/** Carries, in addition to a regular error message, a copy of the failed query
 * and (if available) the SQLSTATE value accompanying the error.
 */
class SqlError : public Failure
{
    /// Query string.  Empty if unknown.
    const std::string query_;
    /// SQLSTATE string describing the error type, if known; or empty string.
    const std::string sqlState_;

    const int errcode_{0};
    const int extendedErrcode_{0};

  public:
    DROGON_EXPORT explicit SqlError(const std::string &msg = "",
                                    const std::string &Q = "",
                                    const char sqlstate[] = nullptr);
    DROGON_EXPORT explicit SqlError(const std::string &msg,
                                    const std::string &Q,
                                    const int errcode,
                                    const int extendedErrcode);
    DROGON_EXPORT virtual ~SqlError() noexcept;

    /// The query whose execution triggered the exception
    DROGON_EXPORT const std::string &query() const noexcept;
    DROGON_EXPORT const std::string &sqlState() const noexcept;
    DROGON_EXPORT int errcode() const noexcept;
    DROGON_EXPORT int extendedErrcode() const noexcept;
};

/// "Help, I don't know whether transaction was committed successfully!"
/** Exception that might be thrown in rare cases where the connection to the
 * database is lost while finishing a database transaction, and there's no way
 * of telling whether it was actually executed by the backend.  In this case
 * the database is left in an indeterminate (but consistent) state, and only
 * manual inspection will tell which is the case.
 */
class InDoubtError : public Failure
{
  public:
    DROGON_EXPORT explicit InDoubtError(const std::string &);
};

/// The backend saw itself forced to roll back the ongoing transaction.
class TransactionRollback : public Failure
{
  public:
    DROGON_EXPORT explicit TransactionRollback(const std::string &);
};

/// Transaction failed to serialize.  Please retry it.
/** Can only happen at transaction isolation levels REPEATABLE READ and
 * SERIALIZABLE.
 *
 * The current transaction cannot be committed without violating the guarantees
 * made by its isolation level.  This is the effect of a conflict with another
 * ongoing transaction.  The transaction may still succeed if you try to
 * perform it again.
 */
class SerializationFailure : public TransactionRollback
{
  public:
    DROGON_EXPORT explicit SerializationFailure(const std::string &);
};

/// We can't tell whether our last statement succeeded.
class StatementCompletionUnknown : public TransactionRollback
{
  public:
    DROGON_EXPORT explicit StatementCompletionUnknown(const std::string &);
};

/// The ongoing transaction has deadlocked.  Retrying it may help.
class DeadlockDetected : public TransactionRollback
{
  public:
    DROGON_EXPORT explicit DeadlockDetected(const std::string &);
};

/// Internal error in internal library
class InternalError : public DrogonDbException, public std::logic_error
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    DROGON_EXPORT explicit InternalError(const std::string &);
};

/// Timeout error in when executing the SQL statement.
class TimeoutError : public DrogonDbException, public std::logic_error
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    DROGON_EXPORT explicit TimeoutError(const std::string &);
};

/// Error in usage of drogon orm library, similar to std::logic_error
class UsageError : public DrogonDbException, public std::logic_error
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    DROGON_EXPORT explicit UsageError(const std::string &);
};

/// Invalid argument passed to drogon orm lib, similar to std::invalid_argument
class ArgumentError : public DrogonDbException, public std::invalid_argument
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    DROGON_EXPORT explicit ArgumentError(const std::string &);
};

/// Value conversion failed, e.g. when converting "Hello" to int.
class ConversionError : public DrogonDbException, public std::domain_error
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    DROGON_EXPORT explicit ConversionError(const std::string &);
};

/// Something is out of range, similar to std::out_of_range
class RangeError : public DrogonDbException, public std::out_of_range
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    DROGON_EXPORT explicit RangeError(const std::string &);
};

/// Query returned an unexpected number of rows.
class UnexpectedRows : public RangeError
{
    const std::exception &base() const noexcept override
    {
        return *this;
    }

  public:
    explicit UnexpectedRows(const std::string &msg) : RangeError(msg)
    {
    }
};

/// Database feature not supported in current setup
class FeatureNotSupported : public SqlError
{
  public:
    explicit FeatureNotSupported(const std::string &err,
                                 const std::string &Q = "",
                                 const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit FeatureNotSupported(const std::string &msg,
                                 const std::string &Q,
                                 const int errcode,
                                 const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

/// Error in data provided to SQL statement
class DataException : public SqlError
{
  public:
    explicit DataException(const std::string &err,
                           const std::string &Q = "",
                           const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit DataException(const std::string &msg,
                           const std::string &Q,
                           const int errcode,
                           const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class IntegrityConstraintViolation : public SqlError
{
  public:
    explicit IntegrityConstraintViolation(const std::string &err,
                                          const std::string &Q = "",
                                          const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit IntegrityConstraintViolation(const std::string &msg,
                                          const std::string &Q,
                                          const int errcode,
                                          const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class RestrictViolation : public IntegrityConstraintViolation
{
  public:
    explicit RestrictViolation(const std::string &err,
                               const std::string &Q = "",
                               const char sqlstate[] = nullptr)
        : IntegrityConstraintViolation(err, Q, sqlstate)
    {
    }

    explicit RestrictViolation(const std::string &msg,
                               const std::string &Q,
                               const int errcode,
                               const int extendedErrcode)
        : IntegrityConstraintViolation(msg, Q, errcode, extendedErrcode)
    {
    }
};

class NotNullViolation : public IntegrityConstraintViolation
{
  public:
    explicit NotNullViolation(const std::string &err,
                              const std::string &Q = "",
                              const char sqlstate[] = nullptr)
        : IntegrityConstraintViolation(err, Q, sqlstate)
    {
    }

    explicit NotNullViolation(const std::string &msg,
                              const std::string &Q,
                              const int errcode,
                              const int extendedErrcode)
        : IntegrityConstraintViolation(msg, Q, errcode, extendedErrcode)
    {
    }
};

class ForeignKeyViolation : public IntegrityConstraintViolation
{
  public:
    explicit ForeignKeyViolation(const std::string &err,
                                 const std::string &Q = "",
                                 const char sqlstate[] = nullptr)
        : IntegrityConstraintViolation(err, Q, sqlstate)
    {
    }

    explicit ForeignKeyViolation(const std::string &msg,
                                 const std::string &Q,
                                 const int errcode,
                                 const int extendedErrcode)
        : IntegrityConstraintViolation(msg, Q, errcode, extendedErrcode)
    {
    }
};

class UniqueViolation : public IntegrityConstraintViolation
{
  public:
    explicit UniqueViolation(const std::string &err,
                             const std::string &Q = "",
                             const char sqlstate[] = nullptr)
        : IntegrityConstraintViolation(err, Q, sqlstate)
    {
    }

    explicit UniqueViolation(const std::string &msg,
                             const std::string &Q,
                             const int errcode,
                             const int extendedErrcode)
        : IntegrityConstraintViolation(msg, Q, errcode, extendedErrcode)
    {
    }
};

class CheckViolation : public IntegrityConstraintViolation
{
  public:
    explicit CheckViolation(const std::string &err,
                            const std::string &Q = "",
                            const char sqlstate[] = nullptr)
        : IntegrityConstraintViolation(err, Q, sqlstate)
    {
    }

    explicit CheckViolation(const std::string &msg,
                            const std::string &Q,
                            const int errcode,
                            const int extendedErrcode)
        : IntegrityConstraintViolation(msg, Q, errcode, extendedErrcode)
    {
    }
};

class InvalidCursorState : public SqlError
{
  public:
    explicit InvalidCursorState(const std::string &err,
                                const std::string &Q = "",
                                const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit InvalidCursorState(const std::string &msg,
                                const std::string &Q,
                                const int errcode,
                                const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class InvalidSqlStatementName : public SqlError
{
  public:
    explicit InvalidSqlStatementName(const std::string &err,
                                     const std::string &Q = "",
                                     const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit InvalidSqlStatementName(const std::string &msg,
                                     const std::string &Q,
                                     const int errcode,
                                     const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class InvalidCursorName : public SqlError
{
  public:
    explicit InvalidCursorName(const std::string &err,
                               const std::string &Q = "",
                               const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit InvalidCursorName(const std::string &msg,
                               const std::string &Q,
                               const int errcode,
                               const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class SyntaxError : public SqlError
{
  public:
    /// Approximate position in string where error occurred, or -1 if unknown.
    const int errorPosition_;

    explicit SyntaxError(const std::string &err,
                         const std::string &Q = "",
                         const char sqlstate[] = nullptr,
                         int pos = -1)
        : SqlError(err, Q, sqlstate), errorPosition_(pos)
    {
    }

    explicit SyntaxError(const std::string &msg,
                         const std::string &Q,
                         const int errcode,
                         const int extendedErrcode,
                         int pos = -1)
        : SqlError(msg, Q, errcode, extendedErrcode), errorPosition_(pos)
    {
    }
};

class UndefinedColumn : public SyntaxError
{
  public:
    explicit UndefinedColumn(const std::string &err,
                             const std::string &Q = "",
                             const char sqlstate[] = nullptr)
        : SyntaxError(err, Q, sqlstate)
    {
    }

    explicit UndefinedColumn(const std::string &msg,
                             const std::string &Q,
                             const int errcode,
                             const int extendedErrcode)
        : SyntaxError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class UndefinedFunction : public SyntaxError
{
  public:
    explicit UndefinedFunction(const std::string &err,
                               const std::string &Q = "",
                               const char sqlstate[] = nullptr)
        : SyntaxError(err, Q, sqlstate)
    {
    }

    explicit UndefinedFunction(const std::string &msg,
                               const std::string &Q,
                               const int errcode,
                               const int extendedErrcode)
        : SyntaxError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class UndefinedTable : public SyntaxError
{
  public:
    explicit UndefinedTable(const std::string &err,
                            const std::string &Q = "",
                            const char sqlstate[] = nullptr)
        : SyntaxError(err, Q, sqlstate)
    {
    }

    explicit UndefinedTable(const std::string &msg,
                            const std::string &Q,
                            const int errcode,
                            const int extendedErrcode)
        : SyntaxError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class InsufficientPrivilege : public SqlError
{
  public:
    explicit InsufficientPrivilege(const std::string &err,
                                   const std::string &Q = "",
                                   const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit InsufficientPrivilege(const std::string &msg,
                                   const std::string &Q,
                                   const int errcode,
                                   const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

/// Resource shortage on the server
class InsufficientResources : public SqlError
{
  public:
    explicit InsufficientResources(const std::string &err,
                                   const std::string &Q = "",
                                   const char sqlstate[] = nullptr)
        : SqlError(err, Q, sqlstate)
    {
    }

    explicit InsufficientResources(const std::string &msg,
                                   const std::string &Q,
                                   const int errcode,
                                   const int extendedErrcode)
        : SqlError(msg, Q, errcode, extendedErrcode)
    {
    }
};

class DiskFull : public InsufficientResources
{
  public:
    explicit DiskFull(const std::string &err,
                      const std::string &Q = "",
                      const char sqlstate[] = nullptr)
        : InsufficientResources(err, Q, sqlstate)
    {
    }

    explicit DiskFull(const std::string &msg,
                      const std::string &Q,
                      const int errcode,
                      const int extendedErrcode)
        : InsufficientResources(msg, Q, errcode, extendedErrcode)
    {
    }
};

class OutOfMemory : public InsufficientResources
{
  public:
    explicit OutOfMemory(const std::string &err,
                         const std::string &Q = "",
                         const char sqlstate[] = nullptr)
        : InsufficientResources(err, Q, sqlstate)
    {
    }

    explicit OutOfMemory(const std::string &msg,
                         const std::string &Q,
                         const int errcode,
                         const int extendedErrcode)
        : InsufficientResources(msg, Q, errcode, extendedErrcode)
    {
    }
};

class TooManyConnections : public BrokenConnection
{
  public:
    explicit TooManyConnections(const std::string &err) : BrokenConnection(err)
    {
    }
};

// /// PL/pgSQL error
// /** Exceptions derived from this class are errors from PL/pgSQL procedures.
//  */
// class PQXX_LIBEXPORT plpgsql_error : public sql_error
// {
// public:
//   explicit plpgsql_error(
//       const std::string &err,
//       const std::string &Q = "",
//       const char sqlstate[] = nullptr) : sql_error(err, Q, sqlstate) {}
// };

// /// Exception raised in PL/pgSQL procedure
// class PQXX_LIBEXPORT plpgsql_raise : public plpgsql_error
// {
// public:
//   explicit plpgsql_raise(
//       const std::string &err,
//       const std::string &Q = "",
//       const char sqlstate[] = nullptr) : plpgsql_error(err, Q, sqlstate) {}
// };

// class PQXX_LIBEXPORT plpgsql_no_data_found : public plpgsql_error
// {
// public:
//   explicit plpgsql_no_data_found(
//       const std::string &err,
//       const std::string &Q = "",
//       const char sqlstate[] = nullptr) : plpgsql_error(err, Q, sqlstate) {}
// };

// class PQXX_LIBEXPORT plpgsql_too_many_rows : public plpgsql_error
// {
// public:
//   explicit plpgsql_too_many_rows(
//       const std::string &err,
//       const std::string &Q = "",
//       const char sqlstate[] = nullptr) : plpgsql_error(err, Q, sqlstate) {}
// };
using DrogonDbExceptionCallback =
    std::function<void(const DrogonDbException &)>;
}  // namespace orm
}  // namespace drogon
