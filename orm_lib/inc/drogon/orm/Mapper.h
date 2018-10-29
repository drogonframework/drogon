/**
 *
 *  Mapper.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Criteria.h>

namespace drogon
{
namespace orm
{

enum class SortOrder
{
    ASC,
    DESC
};
template <typename T>
class Mapper
{
  public:
    Mapper<T> &limit(size_t limit);
    Mapper<T> &offset(size_t offset);
    Mapper<T> &orderBy(const std::string &colName, const SortOrder &order = SortOrder::ASC);
    Mapper<T> &orderBy(size_t colIndex, const SortOrder &order = SortOrder::ASC);

    typedef std::function<void(T)> SingleRowCallback;
    typedef std::function<void(std::vector<T>)> MultipleRowsCallback;
    typedef std::function<void(const sise_t)> CountCallback;

    Mapper(const DbClient &client) : _client(client) {}

    T findByPrimaryKey(const T::PrimaryKeyType &key) noexcept(false);
    void findByPrimaryKey(const T::PrimaryKeyType &key,
                          const SingleRowCallback &rcb,
                          const ExceptionCallback &ecb) noexcept;
    std::future<T> findFutureByPrimaryKey(const T::PrimaryKeyType &key) noexcept;

    std::vector<T> findAll() noexcept(false);
    void findAll(const MultipleRowsCallback &rcb,
                 const ExceptionCallback &ecb) noexcept;
    std::future<std::vector<T>> findFutureAll() noexcept;

    size_t count(const Criteria &criteria) noexcept(false);
    void count(const Criteria &criteria,
               const CountCallback &rcb,
               const ExceptionCallback &ecb) noexcept;
    std::future<size_t> count(const Criteria &criteria) noexcept;

    T findOne(const Criteria &criteria) noexcept(false);
    void findOne(const Criteria &criteria,
                 const SingleRowCallback &rcb,
                 const ExceptionCallback &ecb) noexcept;
    std::future<T> findFutureOne(const Criteria &criteria) noexcept;

    std::vector<T> findBy(const Criteria &criteria) noexcept(false);
    void findBy(const Criteria &criteria,
                const MultipleRowsCallback &rcb,
                const ExceptionCallback &ecb) noexcept;
    std::future<std::vector<T>> findFutureBy(const Criteria &criteria) noexcept;

    void insert(T &obj) noexcept(false);
    void insert(const T &obj,
                const SingleRowCallback &rcb,
                const ExceptionCallback &ecb) noexcept;
    std::future<T> insert(const T &) noexcept;

    size_t update(T &obj) noexcept(false);
    void update(const T &obj,
                const CountCallback &rcb,
                const ExceptionCallback &ecb) noexcept;
    std::future<size_t> update(const T &obj) noexcept;

  private:
    Dbclient &_client;
    std::string getSqlForFindingByPrimaryKey();
};

template <typename T>
inline T Mapper<T>::findByPrimaryKey(const T::PrimaryKeyType &key) noexcept(false)
{
    static_assert(T::hasPrimaryKey, "No primary key in the table!");
    return findOne(Criteria(T::primaryKeyName, key));
}

template <typename T>
inline void Mapper<T>::findByPrimaryKey(const T::PrimaryKeyType &key,
                                        const SingleRowCallback &rcb,
                                        const ExceptionCallback &ecb) noexcept
{
    static_assert(T::hasPrimaryKey, "No primary key in the table!");
    findOne(Criteria(T::primaryKeyName, key), rcb, ecb);
}

template <typename T>
inline std::future<T> Mapper<T>::findFutureByPrimaryKey(const T::PrimaryKeyType &key) noexcept
{
    static_assert(T::hasPrimaryKey, "No primary key in the table!");
    return findFutureOne(Criteria(T::primaryKeyName, key));
}

template <typename T>
inline T Mapper<T>::findOne(const Criteria &criteria) noexcept(false)
{
    std::string sql = "select * from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = _client.replaceSqlPlaceHolder(sql, "$?");
    }
    Result r(nullptr);
    {
        auto binder = _client << sql;
        if (criteria)
            criteria.outputArgs(binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //exec may be throw exception;
    }
    if (r.size() == 0)
    {
        throw Failure("0 rows found");
    }
    else if (r.size() > 1)
    {
        throw Failure("Found more than one row");
    }
    auto row = r[0];
    return T(row);
}

template <typename T>
inline void Mapper<T>::findOne(const Criteria &criteria,
                               const SingleRowCallback &rcb,
                               const ExceptionCallback &ecb) noexcept
{
    std::string sql = "select * from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = _client.replaceSqlPlaceHolder(sql, "$?");
    }

    auto binder = _client << sql;
    if (criteria)
        criteria.outputArgs(binder);
    binder >> [=](const Result &r) {
        if (r.size() == 0)
        {
            ecb(Failure("0 rows found"));
        }
        else if (r.size() > 1)
        {
            ech(Failure("Found more than one row"));
        }
        else
        {
            rcb(T(r[0]));
        }
    };
    binder >> ecb;
}

template <typename T>
inline std::future<T> Mapper<T>::findFutureOne(const Criteria &criteria) noexcept
{
    std::string sql = "select * from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = _client.replaceSqlPlaceHolder(sql, "$?");
    }

    auto binder = _client << sql;
    if (criteria)
        criteria.outputArgs(binder);

    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    binder >> [=](const Result &r) {
        if (r.size() == 0)
        {
            try
            {
                throw Failure("0 rows found");
            }
            catch (...)
            {
                prom->set_exception(std::current_exception());
            }
        }
        else if (r.size() > 1)
        {
            try
            {
                throw Failure("Found more than one row");
            }
            catch (...)
            {
                prom->set_exception(std::current_exception());
            }
        }
        else
        {
            prom->set_value(T(r[0]));
        }
    };
    binder >> [=](const std::exception_ptr &e) {
        prom->set_exception(e);
    };
    binder.exec();
    return prom->get_future();
}

} // namespace orm
} // namespace drogon
