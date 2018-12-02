/**
 *
 *  Mapper.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Criteria.h>
#include <drogon/utils/Utilities.h>
#include <vector>
#include <string>
#include <type_traits>

namespace drogon
{
namespace orm
{

enum class SortOrder
{
    ASC,
    DESC
};
namespace internal
{
template <typename T, bool hasPrimaryKey = true>
struct Traits
{
    typedef typename T::PrimaryKeyType type;
};
template <typename T>
struct Traits<T, false>
{
    typedef int type;
};
} // namespace internal

template <typename T>
class Mapper
{
  public:
    Mapper<T> &limit(size_t limit);
    Mapper<T> &offset(size_t offset);
    Mapper<T> &orderBy(const std::string &colName, const SortOrder &order = SortOrder::ASC);
    Mapper<T> &orderBy(size_t colIndex, const SortOrder &order = SortOrder::ASC);
    Mapper<T> &forUpdate();

    typedef std::function<void(T)> SingleRowCallback;
    typedef std::function<void(std::vector<T>)> MultipleRowsCallback;
    typedef std::function<void(const size_t)> CountCallback;

    Mapper(const DbClientPtr &client) : _client(client) {}

    typedef typename internal::Traits<T, !std::is_same<typename T::PrimaryKeyType, void>::value>::type TraitsPKType;

    T findByPrimaryKey(const TraitsPKType &key) noexcept(false);

    void findByPrimaryKey(const TraitsPKType &key,
                          const SingleRowCallback &rcb,
                          const ExceptionCallback &ecb) noexcept;

    std::future<T> findFutureByPrimaryKey(const TraitsPKType &key) noexcept;

    std::vector<T> findAll() noexcept(false);
    void findAll(const MultipleRowsCallback &rcb,
                 const ExceptionCallback &ecb) noexcept;
    std::future<std::vector<T>> findFutureAll() noexcept;

    size_t count(const Criteria &criteria = Criteria()) noexcept(false);
    void count(const Criteria &criteria,
               const CountCallback &rcb,
               const ExceptionCallback &ecb) noexcept;
    std::future<size_t> countFuture(const Criteria &criteria = Criteria()) noexcept;

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
    std::future<T> insertFuture(const T &) noexcept;

    size_t update(T &obj) noexcept(false);
    void update(const T &obj,
                const CountCallback &rcb,
                const ExceptionCallback &ecb) noexcept;
    std::future<size_t> updateFuture(const T &obj) noexcept;

    size_t deleteOne(const T &obj) noexcept(false);
    void deleteOne(const T &obj,
                   const CountCallback &rcb,
                   const ExceptionCallback &ecb) noexcept;
    std::future<size_t> deleteFutureOne(const T &obj) noexcept;

    size_t deleteBy(const Criteria &criteria) noexcept(false);
    void deleteBy(const Criteria &criteria,
                  const CountCallback &rcb,
                  const ExceptionCallback &ecb) noexcept;
    std::future<size_t> deleteFutureBy(const Criteria &criteria) noexcept;

  private:
    DbClientPtr _client;
    std::string _limitString;
    std::string _offsetString;
    std::string _orderbyString;
    bool _forUpdate = false;
    void clear()
    {
        _limitString.clear();
        _offsetString.clear();
        _orderbyString.clear();
        _forUpdate = false;
    }
    template <typename PKType = decltype(T::primaryKeyName)>
    typename std::enable_if<std::is_same<const std::string, PKType>::value, void>::type makePrimaryKeyCriteria(std::string &sql)
    {
        sql += " where ";
        sql += T::primaryKeyName;
        sql += " = $?";
    }
    template <typename PKType = decltype(T::primaryKeyName)>
    typename std::enable_if<std::is_same<const std::vector<std::string>, PKType>::value, void>::type makePrimaryKeyCriteria(std::string &sql)
    {
        sql += " where ";
        for (size_t i = 0; i < T::primaryKeyName.size(); i++)
        {
            sql += T::primaryKeyName[i];
            sql += " = $?";
            if (i < (T::primaryKeyName.size() - 1))
            {
                sql += " and ";
            }
        }
    }
    template <typename PKType = decltype(T::primaryKeyName)>
    typename std::enable_if<std::is_same<const std::string, PKType>::value, void>::type
    outputPrimeryKeyToBinder(const TraitsPKType &pk, internal::SqlBinder &binder)
    {
        binder << pk;
    }
    template <typename PKType = decltype(T::primaryKeyName)>
    typename std::enable_if<std::is_same<const std::vector<std::string>, PKType>::value, void>::type
    outputPrimeryKeyToBinder(const TraitsPKType &pk, internal::SqlBinder &binder)
    {
        tupleToBinder<typename T::PrimaryKeyType>(pk, binder);
    }

    template <typename TP, ssize_t N = std::tuple_size<TP>::value>
    typename std::enable_if<(N > 1), void>::type tupleToBinder(const TP &t, internal::SqlBinder &binder)
    {
        tupleToBinder<TP, N - 1>(t, binder);
        binder << std::get<N - 1>(t);
    }
    template <typename TP, ssize_t N = std::tuple_size<TP>::value>
    typename std::enable_if<(N == 1), void>::type tupleToBinder(const TP &t, internal::SqlBinder &binder)
    {
        binder << std::get<0>(t);
    }

    std::string replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const;
};

template <typename T>
inline T Mapper<T>::findByPrimaryKey(const typename Mapper<T>::TraitsPKType &key) noexcept(false)
{
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    // return findOne(Criteria(T::primaryKeyName, key));
    std::string sql = "select * from ";
    sql += T::tableName;
    makePrimaryKeyCriteria(sql);
    if (_forUpdate)
    {
        sql += " for update";
    }
    sql = replaceSqlPlaceHolder(sql, "$?");
    clear();
    Result r(nullptr);
    {
        auto binder = *_client << sql;
        outputPrimeryKeyToBinder(key, binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //exec may be throw exception;
    }
    if (r.size() == 0)
    {
        throw UnexpectedRows("0 rows found");
    }
    else if (r.size() > 1)
    {
        throw UnexpectedRows("Found more than one row");
    }
    auto row = r[0];
    return T(row);
}

template <typename T>
inline void Mapper<T>::findByPrimaryKey(const typename Mapper<T>::TraitsPKType &key,
                                        const SingleRowCallback &rcb,
                                        const ExceptionCallback &ecb) noexcept
{
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    // findOne(Criteria(T::primaryKeyName, key), rcb, ecb);
    std::string sql = "select * from ";
    sql += T::tableName;
    makePrimaryKeyCriteria(sql);
    if (_forUpdate)
    {
        sql += " for update";
    }
    sql = replaceSqlPlaceHolder(sql, "$?");
    clear();
    auto binder = *_client << sql;
    outputPrimeryKeyToBinder(key, binder);
    binder >> [=](const Result &r) {
        if (r.size() == 0)
        {
            ecb(UnexpectedRows("0 rows found"));
        }
        else if (r.size() > 1)
        {
            ecb(UnexpectedRows("Found more than one row"));
        }
        else
        {
            rcb(T(r[0]));
        }
    };
    binder >> ecb;
}

template <typename T>
inline std::future<T>
Mapper<T>::findFutureByPrimaryKey(const typename Mapper<T>::TraitsPKType &key) noexcept
{
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    //return findFutureOne(Criteria(T::primaryKeyName, key));
    std::string sql = "select * from ";
    sql += T::tableName;
    makePrimaryKeyCriteria(sql);
    if (_forUpdate)
    {
        sql += " for update";
    }
    sql = replaceSqlPlaceHolder(sql, "$?");
    clear();
    auto binder = *_client << sql;
    outputPrimeryKeyToBinder(key, binder);

    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    binder >> [=](const Result &r) {
        if (r.size() == 0)
        {
            try
            {
                throw UnexpectedRows("0 rows found");
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
                throw UnexpectedRows("Found more than one row");
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

template <typename T>
inline T Mapper<T>::findOne(const Criteria &criteria) noexcept(false)
{
    std::string sql = "select * from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    sql.append(_orderbyString).append(_offsetString).append(_limitString);
    if (_forUpdate)
    {
        sql += " for update";
    }
    clear();
    Result r(nullptr);
    {
        auto binder = *_client << sql;
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
        throw UnexpectedRows("0 rows found");
    }
    else if (r.size() > 1)
    {
        throw UnexpectedRows("Found more than one row");
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
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    sql.append(_orderbyString).append(_offsetString).append(_limitString);
    if (_forUpdate)
    {
        sql += " for update";
    }
    clear();
    auto binder = *_client << sql;
    if (criteria)
        criteria.outputArgs(binder);
    binder >> [=](const Result &r) {
        if (r.size() == 0)
        {
            ecb(UnexpectedRows("0 rows found"));
        }
        else if (r.size() > 1)
        {
            ecb(UnexpectedRows("Found more than one row"));
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
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    sql.append(_orderbyString).append(_offsetString).append(_limitString);
    if (_forUpdate)
    {
        sql += " for update";
    }
    clear();
    auto binder = *_client << sql;
    if (criteria)
        criteria.outputArgs(binder);

    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    binder >> [=](const Result &r) {
        if (r.size() == 0)
        {
            try
            {
                throw UnexpectedRows("0 rows found");
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
                throw UnexpectedRows("Found more than one row");
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
template <typename T>
inline std::vector<T> Mapper<T>::findBy(const Criteria &criteria) noexcept(false)
{
    std::string sql = "select * from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    sql.append(_orderbyString).append(_offsetString).append(_limitString);
    if (_forUpdate)
    {
        sql += " for update";
    }
    clear();
    Result r(nullptr);
    {
        auto binder = *_client << sql;
        if (criteria)
            criteria.outputArgs(binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //exec may be throw exception;
    }
    std::vector<T> ret;
    for (auto row : r)
    {
        ret.push_back(T(row));
    }
    return ret;
}
template <typename T>
inline void Mapper<T>::findBy(const Criteria &criteria,
                              const MultipleRowsCallback &rcb,
                              const ExceptionCallback &ecb) noexcept
{
    std::string sql = "select * from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    sql.append(_orderbyString).append(_offsetString).append(_limitString);
    if (_forUpdate)
    {
        sql += " for update";
    }
    clear();
    auto binder = *_client << sql;
    if (criteria)
        criteria.outputArgs(binder);
    binder >> [=](const Result &r) {
        std::vector<T> ret;
        for (auto row : r)
        {
            ret.push_back(T(row));
        }
        rcb(ret);
    };
    binder >> ecb;
}
template <typename T>
inline std::future<std::vector<T>> Mapper<T>::findFutureBy(const Criteria &criteria) noexcept
{
    std::string sql = "select * from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    sql.append(_orderbyString).append(_offsetString).append(_limitString);
    if (_forUpdate)
    {
        sql += " for update";
    }
    clear();
    auto binder = *_client << sql;
    if (criteria)
        criteria.outputArgs(binder);

    std::shared_ptr<std::promise<std::vector<T>>> prom = std::make_shared<std::promise<std::vector<T>>>();
    binder >> [=](const Result &r) {
        std::vector<T> ret;
        for (auto row : r)
        {
            ret.push_back(T(row));
        }
        prom->set_value(ret);
    };
    binder >> [=](const std::exception_ptr &e) {
        prom->set_exception(e);
    };
    binder.exec();
    return prom->get_future();
}
template <typename T>
inline std::vector<T> Mapper<T>::findAll() noexcept(false)
{
    return findBy(Criteria());
}
template <typename T>
inline void Mapper<T>::findAll(const MultipleRowsCallback &rcb,
                               const ExceptionCallback &ecb) noexcept
{
    findBy(Criteria(), rcb, ecb);
}
template <typename T>
inline std::future<std::vector<T>> Mapper<T>::findFutureAll() noexcept
{
    return findFutureBy(Criteria());
}
template <typename T>
inline size_t Mapper<T>::count(const Criteria &criteria) noexcept(false)
{
    std::string sql = "select count(*) from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    clear();
    Result r(nullptr);
    {
        auto binder = *_client << sql;
        if (criteria)
            criteria.outputArgs(binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //exec may be throw exception;
    }
    assert(r.size() == 1);
    return r[0]["count"].as<size_t>();
}
template <typename T>
inline void Mapper<T>::count(const Criteria &criteria,
                             const CountCallback &rcb,
                             const ExceptionCallback &ecb) noexcept
{
    std::string sql = "select count(*) from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    clear();
    auto binder = *_client << sql;
    if (criteria)
        criteria.outputArgs(binder);
    binder >> [=](const Result &r) {
        assert(r.size() == 1);
        rcb(r[0]["count"].as<size_t>());
    };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::countFuture(const Criteria &criteria) noexcept
{
    std::string sql = "select count(*) from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    clear();
    auto binder = *_client << sql;
    if (criteria)
        criteria.outputArgs(binder);

    std::shared_ptr<std::promise<size_t>> prom = std::make_shared<std::promise<size_t>>();
    binder >> [=](const Result &r) {
        assert(r.size() == 1);
        prom->set_value(r[0]["count"].as<size_t>());
    };
    binder >> [=](const std::exception_ptr &e) {
        prom->set_exception(e);
    };
    binder.exec();
    return prom->get_future();
}
template <typename T>
inline void Mapper<T>::insert(T &obj) noexcept(false)
{
    clear();
    std::string sql = "insert into ";
    sql += T::tableName;
    sql += " (";
    for (auto colName : T::insertColumns())
    {
        sql += colName;
        sql += ",";
    }
    sql[sql.length() - 1] = ')'; //Replace the last ','
    sql += " values (";
    for (int i = 0; i < T::insertColumns().size(); i++)
    {
        sql += "$?,";
    }
    sql[sql.length() - 1] = ')'; //Replace the last ','
    if (_client->type() == ClientType::PostgreSQL)
    {
        sql += " returning *";
    }
    sql = replaceSqlPlaceHolder(sql, "$?");
    Result r(nullptr);
    {
        auto binder = *_client << sql;
        obj.outputArgs(binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //Maybe throw exception;
    }
    if (_client->type() == ClientType::PostgreSQL)
    {
        assert(r.size() == 1);
        obj = T(r[0]);
    }
    else if (_client->type() == ClientType::Mysql)
    {
        auto id = r.insertId();
        obj.updateId(id);
    }
}
template <typename T>
inline void Mapper<T>::insert(const T &obj,
                              const SingleRowCallback &rcb,
                              const ExceptionCallback &ecb) noexcept
{
    clear();
    std::string sql = "insert into ";
    sql += T::tableName;
    sql += " (";
    for (auto colName : T::insertColumns())
    {
        sql += colName;
        sql += ",";
    }
    sql[sql.length() - 1] = ')'; //Replace the last ','
    sql += " values (";
    for (int i = 0; i < T::insertColumns().size(); i++)
    {
        sql += "$?,";
    }
    sql[sql.length() - 1] = ')'; //Replace the last ','
    if (_client->type() == ClientType::PostgreSQL)
    {
        sql += " returning *";
    }
    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *_client << sql;
    obj.outputArgs(binder);
    binder >> [=](const Result &r) {
        if (_client->type() == ClientType::PostgreSQL)
        {
            assert(r.size() == 1);
            rcb(T(r[0]));
        }
        else if (_client->type() == ClientType::Mysql)
        {
            auto id = r.insertId();
            auto newObj = obj;
            newObj.updateId(id);
            rcb(newObj);
        }
    };
    binder >> ecb;
}
template <typename T>
inline std::future<T> Mapper<T>::insertFuture(const T &obj) noexcept
{
    clear();
    std::string sql = "insert into ";
    sql += T::tableName;
    sql += " (";
    for (auto colName : T::insertColumns())
    {
        sql += colName;
        sql += ",";
    }
    sql[sql.length() - 1] = ')'; //Replace the last ','
    sql += " values (";
    for (int i = 0; i < T::insertColumns().size(); i++)
    {
        sql += "$?,";
    }
    sql[sql.length() - 1] = ')'; //Replace the last ','
    if (_client->type() == ClientType::PostgreSQL)
    {
        sql += " returning *";
    }
    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *_client << sql;
    obj.outputArgs(binder);

    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    binder >> [=](const Result &r) {
        if (_client->type() == ClientType::PostgreSQL)
        {
            assert(r.size() == 1);
            prom->set_value(T(r[0]));
        }
        else if (_client->type() == ClientType::Mysql)
        {
            auto id = r.insertId();
            auto newObj = obj;
            newObj.updateId(id);
            prom->set_value(newObj);
        }
    };
    binder >> [=](const std::exception_ptr &e) {
        prom->set_exception(e);
    };
    binder.exec();
    return prom->get_future();
}
template <typename T>
inline size_t Mapper<T>::update(T &obj) noexcept(false)
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "update ";
    sql += T::tableName;
    sql += " set ";
    for (auto colName : obj.updateColumns())
    {
        sql += colName;
        sql += " = $?,";
    }
    sql[sql.length() - 1] = ' '; //Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    Result r(nullptr);
    {
        auto binder = *_client << sql;
        obj.updateArgs(binder);
        outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //Maybe throw exception;
    }
    return r.affectedRows();
}
template <typename T>
inline void Mapper<T>::update(const T &obj,
                              const CountCallback &rcb,
                              const ExceptionCallback &ecb) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "update ";
    sql += T::tableName;
    sql += " set ";
    for (auto colName : obj.updateColumns())
    {
        sql += colName;
        sql += " = $?,";
    }
    sql[sql.length() - 1] = ' '; //Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *_client << sql;
    obj.updateArgs(binder);
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
    binder >> [=](const Result &r) {
        rcb(r.affectedRows());
    };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::updateFuture(const T &obj) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "update ";
    sql += T::tableName;
    sql += " set ";
    for (auto colName : obj.updateColumns())
    {
        sql += colName;
        sql += " = $?,";
    }
    sql[sql.length() - 1] = ' '; //Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *_client << sql;
    obj.updateArgs(binder);
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);

    std::shared_ptr<std::promise<size_t>> prom = std::make_shared<std::promise<size_t>>();
    binder >> [=](const Result &r) {
        prom->set_value(r.affectedRows());
    };
    binder >> [=](const std::exception_ptr &e) {
        prom->set_exception(e);
    };
    binder.exec();
    return prom->get_future();
}

template <typename T>
inline size_t Mapper<T>::deleteOne(const T &obj) noexcept(false)
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;

    sql += " "; //Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    Result r(nullptr);
    {
        auto binder = *_client << sql;
        outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //Maybe throw exception;
    }
    return r.affectedRows();
}
template <typename T>
inline void Mapper<T>::deleteOne(const T &obj,
                                 const CountCallback &rcb,
                                 const ExceptionCallback &ecb) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;
    sql += " ";

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *_client << sql;
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
    binder >> [=](const Result &r) {
        rcb(r.affectedRows());
    };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::deleteFutureOne(const T &obj) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;
    sql += " ";

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *_client << sql;
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);

    std::shared_ptr<std::promise<size_t>> prom = std::make_shared<std::promise<size_t>>();
    binder >> [=](const Result &r) {
        prom->set_value(r.affectedRows());
    };
    binder >> [=](const std::exception_ptr &e) {
        prom->set_exception(e);
    };
    binder.exec();
    return prom->get_future();
}

template <typename T>
inline size_t Mapper<T>::deleteBy(const Criteria &criteria) noexcept(false)
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;

    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }

    Result r(nullptr);
    {
        auto binder = *_client << sql;
        if (criteria)
        {
            criteria.outputArgs(binder);
        }
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) {
            r = result;
        };
        binder.exec(); //Maybe throw exception;
    }
    return r.affectedRows();
}
template <typename T>
inline void Mapper<T>::deleteBy(const Criteria &criteria,
                                const CountCallback &rcb,
                                const ExceptionCallback &ecb) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;

    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }

    auto binder = *_client << sql;
    if (criteria)
    {
        criteria.outputArgs(binder);
    }
    binder >> [=](const Result &r) {
        rcb(r.affectedRows());
    };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::deleteFutureBy(const Criteria &criteria) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value, "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    auto binder = *_client << sql;
    if (criteria)
    {
        criteria.outputArgs(binder);
    }

    std::shared_ptr<std::promise<size_t>> prom = std::make_shared<std::promise<size_t>>();
    binder >> [=](const Result &r) {
        prom->set_value(r.affectedRows());
    };
    binder >> [=](const std::exception_ptr &e) {
        prom->set_exception(e);
    };
    binder.exec();
    return prom->get_future();
}

template <typename T>
inline Mapper<T> &Mapper<T>::limit(size_t limit)
{
    assert(limit > 0);
    if (limit > 0)
    {
        _limitString = formattedString(" limit %u", limit);
    }
    return *this;
}
template <typename T>
inline Mapper<T> &Mapper<T>::offset(size_t offset)
{
    _offsetString = formattedString(" offset %u", offset);
    return *this;
}
template <typename T>
inline Mapper<T> &Mapper<T>::orderBy(const std::string &colName, const SortOrder &order)
{
    if (_orderbyString.empty())
    {
        _orderbyString = formattedString(" order by %s", colName.c_str());
        if (order == SortOrder::DESC)
        {
            _orderbyString += " desc";
        }
    }
    else
    {
        _orderbyString += ",";
        _orderbyString += colName;
        if (order == SortOrder::DESC)
        {
            _orderbyString += " desc";
        }
    }
    return *this;
}
template <typename T>
inline Mapper<T> &Mapper<T>::orderBy(size_t colIndex, const SortOrder &order)
{
    std::string colName = T::getColumnName(colIndex);
    assert(!colName.empty());
    return orderBy(colName, order);
}
template <typename T>
inline Mapper<T> &Mapper<T>::forUpdate()
{
    _forUpdate = true;
    return *this;
}
template <typename T>
inline std::string Mapper<T>::replaceSqlPlaceHolder(const std::string &sqlStr, const std::string &holderStr) const
{
    ///FIXME add mysql support
    if (_client->type() == ClientType::PostgreSQL)
    {
        std::string::size_type startPos = 0;
        std::string::size_type pos;
        std::stringstream ret;
        size_t phCount = 1;
        do
        {
            pos = sqlStr.find(holderStr, startPos);
            if (pos == std::string::npos)
            {
                ret << sqlStr.substr(startPos);
                return ret.str();
            }
            ret << sqlStr.substr(startPos, pos - startPos);
            ret << "$";
            ret << phCount++;
            startPos = pos + holderStr.length();
        } while (1);
    }
    else if (_client->type() == ClientType::Mysql)
    {
        std::string::size_type startPos = 0;
        std::string::size_type pos;
        std::stringstream ret;
        do
        {
            pos = sqlStr.find(holderStr, startPos);
            if (pos == std::string::npos)
            {
                ret << sqlStr.substr(startPos);
                return ret.str();
            }
            ret << sqlStr.substr(startPos, pos - startPos);
            ret << "?";
            startPos = pos + holderStr.length();
        } while (1);
    }
    else
    {
        return sqlStr;
    }
}

} // namespace orm
} // namespace drogon
