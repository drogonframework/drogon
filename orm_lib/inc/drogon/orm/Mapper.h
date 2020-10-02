/**
 *
 *  @file Mapper.h
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
#include <drogon/orm/Criteria.h>
#include <drogon/orm/DbClient.h>
#include <drogon/utils/Utilities.h>
#include <string>
#include <type_traits>
#include <vector>

#ifdef _WIN32
using ssize_t = std::intptr_t;
#endif

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
    using type = typename T::PrimaryKeyType;
};
template <typename T>
struct Traits<T, false>
{
    using type = int;
};
template <typename T>
struct has_sqlForFindingByPrimaryKey
{
  private:
    using yes = std::true_type;
    using no = std::false_type;

    template <typename U>
    static auto test(int) -> decltype(U::sqlForFindingByPrimaryKey(), yes());

    template <typename>
    static no test(...);

  public:
    static constexpr bool value =
        std::is_same<decltype(test<T>(0)), yes>::value;
};
template <typename T>
struct has_sqlForDeletingByPrimaryKey
{
  private:
    using yes = std::true_type;
    using no = std::false_type;

    template <typename U>
    static auto test(int)
        -> decltype(U::sqlForDeletingByPrimaryKey().length(), yes());

    template <typename>
    static no test(...);

  public:
    static constexpr bool value =
        std::is_same<decltype(test<T>(0)), yes>::value;
};
}  // namespace internal

/**
 * @brief The mapper template
 *
 * @tparam T The type of the model to be mapped.
 *
 * @details The mapping between the model object and the database table is
 * performed by the Mapper class template. The Mapper class template
 * encapsulates common operations such as adding, deleting, and changing, so
 * that the user can perform the above operations without writing a SQL
 * statement.
 *
 * The construction of the Mapper object is very simple. The template
 * parameter is the type of the model you want to access. The constructor has
 * only one parameter, which is the DbClient smart pointer mentioned earlier. As
 * mentioned earlier, the Transaction class is a subclass of DbClient, so you
 * can also construct a Mapper object with a smart pointer to a transaction,
 * which means that the Mapper mapping also supports transactions.
 *
 * Like DbClient, Mapper also provides asynchronous and synchronous interfaces.
 * The synchronous interface is blocked and may throw an exception. The returned
 * future object is blocked in the get() method and may throw an exception. The
 * normal asynchronous interface does not throw an exception, but returns the
 * result through two callbacks (result callback and exception callback). The
 * type of the exception callback is the same as that in the DbClient interface.
 * The result callback is also divided into several categories according to the
 * interface function.
 */
template <typename T>
class Mapper
{
  public:
    /**
     * @brief Construct a new Mapper object
     *
     * @param client The smart pointer to the database client object.
     */
    Mapper(const DbClientPtr &client) : client_(client)
    {
    }

    /**
     * @brief Add a limit to the query.
     *
     * @param limit The limit
     * @return Mapper<T>& The Mapper itself.
     */
    Mapper<T> &limit(size_t limit);

    /**
     * @brief Add a offset to the query.
     *
     * @param offset The offset.
     * @return Mapper<T>& The Mapper itself.
     */
    Mapper<T> &offset(size_t offset);

    /**
     * @brief Set the order of the results.
     *
     * @param colName the column name, the results are sorted by that column
     * @param order Ascending or descending order
     * @return Mapper<T>& The Mapper itself.
     */
    Mapper<T> &orderBy(const std::string &colName,
                       const SortOrder &order = SortOrder::ASC);

    /**
     * @brief Set the order of the results.
     *
     * @param colIndex the column index, the results are sorted by that column
     * @param order Ascending or descending order
     * @return Mapper<T>& The Mapper itself.
     */
    Mapper<T> &orderBy(size_t colIndex,
                       const SortOrder &order = SortOrder::ASC);

    /**
     * @brief Lock the result for updating.
     *
     * @return Mapper<T>& The Mapper itself.
     */
    Mapper<T> &forUpdate();

    using SingleRowCallback = std::function<void(T)>;
    using MultipleRowsCallback = std::function<void(std::vector<T>)>;
    using CountCallback = std::function<void(const size_t)>;

    using TraitsPKType = typename internal::
        Traits<T, !std::is_same<typename T::PrimaryKeyType, void>::value>::type;

    /**
     * @brief Find a record by the primary key.
     *
     * @param key The value of the primary key.
     * @return T The record of the primary key.
     * @note If no hit record exists, an UnexpectedRows exception is thrown.
     */

    template <typename U = T>
    inline typename std::enable_if<
        !std::is_same<typename U::PrimaryKeyType, void>::value,
        T>::type
    findByPrimaryKey(const TraitsPKType &key) noexcept(false)
    {
        static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                      "No primary key in the table!");
        static_assert(
            internal::has_sqlForFindingByPrimaryKey<T>::value,
            "No function member named sqlForFindingByPrimaryKey, please "
            "make sure that the model class is generated by the latest "
            "version of drogon_ctl");
        // return findOne(Criteria(T::primaryKeyName, key));
        std::string sql = T::sqlForFindingByPrimaryKey();
        if (forUpdate_)
        {
            sql += " for update";
        }
        clear();
        Result r(nullptr);
        {
            auto binder = *client_ << std::move(sql);
            outputPrimeryKeyToBinder(key, binder);
            binder << Mode::Blocking;
            binder >> [&r](const Result &result) { r = result; };
            binder.exec();  // exec may be throw exception;
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

    template <typename U = T>
    inline typename std::enable_if<
        std::is_same<typename U::PrimaryKeyType, void>::value,
        T>::type
    findByPrimaryKey(const TraitsPKType &key) noexcept(false)
    {
        LOG_FATAL << "The table must have a primary key";
        abort();
    }

    /**
     * @brief Asynchronously find a record by the primary key.
     *
     * @param key The value of the primary key.
     * @param rcb Is called when a record is found.
     * @param ecb Is called when an error occurs or a record cannot be found.
     */
    template <typename U = T>
    inline typename std::enable_if<
        !std::is_same<typename U::PrimaryKeyType, void>::value,
        void>::type
    findByPrimaryKey(const TraitsPKType &key,
                     const SingleRowCallback &rcb,
                     const ExceptionCallback &ecb) noexcept
    {
        static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                      "No primary key in the table!");
        static_assert(
            internal::has_sqlForFindingByPrimaryKey<T>::value,
            "No function member named sqlForFindingByPrimaryKey, please "
            "make sure that the model class is generated by the latest "
            "version of drogon_ctl");
        // findOne(Criteria(T::primaryKeyName, key), rcb, ecb);
        std::string sql = T::sqlForFindingByPrimaryKey();
        if (forUpdate_)
        {
            sql += " for update";
        }
        clear();
        auto binder = *client_ << std::move(sql);
        outputPrimeryKeyToBinder(key, binder);
        binder >> [ecb, rcb](const Result &r) {
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
    template <typename U = T>
    inline typename std::enable_if<
        std::is_same<typename U::PrimaryKeyType, void>::value,
        void>::type
    findByPrimaryKey(const TraitsPKType &key,
                     const SingleRowCallback &rcb,
                     const ExceptionCallback &ecb) noexcept
    {
        LOG_FATAL << "The table must have a primary key";
        abort();
    }

    /**
     * @brief Asynchronously find a record by the primary key.
     *
     * @param key The value of the primary key.
     * @return std::future<T> The future object with which user can get the
     * result.
     * @note If no hit record exists, an UnexpectedRows exception is thrown when
     * user calls the get() method of the future object.
     */
    template <typename U = T>
    inline typename std::enable_if<
        !std::is_same<typename U::PrimaryKeyType, void>::value,
        std::future<T>>::type
    findFutureByPrimaryKey(const TraitsPKType &key) noexcept
    {
        static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                      "No primary key in the table!");
        static_assert(
            internal::has_sqlForFindingByPrimaryKey<T>::value,
            "No function member named sqlForFindingByPrimaryKey, please "
            "make sure that the model class is generated by the latest "
            "version of drogon_ctl");
        // return findFutureOne(Criteria(T::primaryKeyName, key));
        std::string sql = T::sqlForFindingByPrimaryKey();
        if (forUpdate_)
        {
            sql += " for update";
        }
        clear();
        auto binder = *client_ << std::move(sql);
        outputPrimeryKeyToBinder(key, binder);

        std::shared_ptr<std::promise<T>> prom =
            std::make_shared<std::promise<T>>();
        binder >> [prom](const Result &r) {
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
        binder >>
            [prom](const std::exception_ptr &e) { prom->set_exception(e); };
        binder.exec();
        return prom->get_future();
    }

    template <typename U = T>
    inline typename std::enable_if<
        std::is_same<typename U::PrimaryKeyType, void>::value,
        std::future<U>>::type
    findFutureByPrimaryKey(const TraitsPKType &key) noexcept
    {
        LOG_FATAL << "The table must have a primary key";
        abort();
    }

    /**
     * @brief Find all the records in the table.
     *
     * @return std::vector<T> The vector of all the records.
     */
    std::vector<T> findAll() noexcept(false);

    /**
     * @brief Asynchronously find all the records in the table.
     *
     * @param rcb is called with the result.
     * @param ecb is called when an error occurs.
     */
    void findAll(const MultipleRowsCallback &rcb,
                 const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Asynchronously find all the records in the table.
     *
     * @return std::future<std::vector<T>> The future object with which user can
     * get the result.
     */
    std::future<std::vector<T>> findFutureAll() noexcept;

    /**
     * @brief Get the count of rows that match the given criteria.
     *
     * @param criteria The criteria.
     * @return size_t The number of rows.
     */
    size_t count(const Criteria &criteria = Criteria()) noexcept(false);

    /**
     * @brief Asynchronously get the number of rows that match the given
     * criteria.
     *
     * @param criteria The criteria.
     * @param rcb is clalled with the result.
     * @param ecb is called when an error occurs.
     */
    void count(const Criteria &criteria,
               const CountCallback &rcb,
               const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Asynchronously get the number of rows that match the given
     * criteria.
     *
     * @param criteria The criteria.
     * @return std::future<size_t> The future object with which user can get the
     * number of rows
     */
    std::future<size_t> countFuture(
        const Criteria &criteria = Criteria()) noexcept;

    /**
     * @brief Find one record that matches the given criteria.
     *
     * @param criteria The criteria.
     * @return T The result record.
     * @note if the number of rows is greater than one or equal to zero, an
     * UnexpectedRows exception is thrown.
     */
    T findOne(const Criteria &criteria) noexcept(false);

    /**
     * @brief Asynchronously find one record that matches the given criteria.
     *
     * @param criteria The criteria.
     * @param rcb is called with the result.
     * @param ecb is called when an error occurs.
     */
    void findOne(const Criteria &criteria,
                 const SingleRowCallback &rcb,
                 const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Asynchronously find one record that matches the given criteria.
     *
     * @param criteria The criteria.
     * @return std::future<T> The future object with which user can get the
     * result.
     * @note if the number of rows is greater than one or equal to zero, an
     * UnexpectedRows exception is thrown when the get() method of the future
     * object is called.
     */
    std::future<T> findFutureOne(const Criteria &criteria) noexcept;

    /**
     * @brief Select the rows that match the given criteria.
     *
     * @param criteria The criteria.
     * @return std::vector<T> The vector of rows that match the given criteria.
     */
    std::vector<T> findBy(const Criteria &criteria) noexcept(false);

    /**
     * @brief Asynchronously select the rows that match the given criteria.
     *
     * @param criteria The criteria.
     * @param rcb is called with the result.
     * @param ecb is called when an error occurs.
     */
    void findBy(const Criteria &criteria,
                const MultipleRowsCallback &rcb,
                const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Asynchronously select the rows that match the given criteria.
     *
     * @param criteria The criteria.
     * @return std::future<std::vector<T>> The future object with which user can
     * get the result.
     */
    std::future<std::vector<T>> findFutureBy(const Criteria &criteria) noexcept;

    /**
     * @brief Insert a row into the table.
     *
     * @param obj The object to be inserted.
     * @note The auto-increased primary key (if it exists) is set to the obj
     * argument after the method returns.
     */
    void insert(T &obj) noexcept(false);

    /**
     * @brief Asynchronously insert a row into the table.
     *
     * @param obj The object to be inserted.
     * @param rcb is called with the result (with the auto-increased primary key
     * (if it exists)).
     * @param ecb is called when an error occurs.
     */
    void insert(const T &obj,
                const SingleRowCallback &rcb,
                const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Asynchronously insert a row into the table.
     *
     * @return std::future<T> The future object with which user can get the
     * result (with the auto-increased primary key (if it exists)).
     */
    std::future<T> insertFuture(const T &) noexcept;

    /**
     * @brief Update a record.
     *
     * @param obj The record.
     * @return size_t The number of updated records. It only could be 0 or 1.
     * @note The table must have a primary key.
     */
    size_t update(const T &obj) noexcept(false);

    /**
     * @brief Asynchronously update a record.
     *
     * @param obj The record.
     * @param rcb is called with the number of updated records.
     * @param ecb is called when an error occurs.
     * @note The table must have a primary key.
     */
    void update(const T &obj,
                const CountCallback &rcb,
                const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Asynchronously update a record.
     *
     * @param obj The record.
     * @return std::future<size_t> The future object with which user can get the
     * number of updated records.
     * @note The table must have a primary key.
     */
    std::future<size_t> updateFuture(const T &obj) noexcept;

    /**
     * @brief Delete a record from the table.
     *
     * @param obj The record.
     * @return size_t The number of deleted records.
     * @note The table must have a primary key.
     */
    size_t deleteOne(const T &obj) noexcept(false);

    /**
     * @brief Asynchronously delete a record from the table.
     *
     * @param obj The record.
     * @param rcb is called with the number of deleted records.
     * @param ecb is called when an error occurs.
     * @note The table must have a primary key.
     */
    void deleteOne(const T &obj,
                   const CountCallback &rcb,
                   const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Asynchronously delete a record from the table.
     *
     * @param obj The record.
     * @return std::future<size_t> The future object with which user can get the
     * number of deleted records.
     * @note The table must have a primary key.
     */
    std::future<size_t> deleteFutureOne(const T &obj) noexcept;

    /**
     * @brief Delete records that satisfy the given criteria.
     *
     * @param criteria The criteria.
     * @return size_t The number of deleted records.
     */
    size_t deleteBy(const Criteria &criteria) noexcept(false);

    /**
     * @brief Delete records that match the given criteria asynchronously.
     *
     * @param criteria The criteria
     * @param rcb is called with the number of deleted records.
     * @param ecb is called when an error occurs.
     */
    void deleteBy(const Criteria &criteria,
                  const CountCallback &rcb,
                  const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Delete records that match the given criteria asynchronously.
     *
     * @param criteria The criteria
     * @return std::future<size_t> The future object with which user can get the
     * number of deleted records
     */
    std::future<size_t> deleteFutureBy(const Criteria &criteria) noexcept;

    /**
     * @brief Delete the record that matches the given primary key.
     *
     * @param key The primary key.
     * @return size_t The number of deleted records (1 or 0).
     */
    size_t deleteByPrimaryKey(const TraitsPKType &key) noexcept(false);

    /**
     * @brief Asynchronously delete the record that matches the given primary
     * key.
     *
     * @param key The primary key.
     * @param rcb is called with the number of deleted records.
     * @param ecb is called when an error occurs.
     */
    void deleteByPrimaryKey(const TraitsPKType &key,
                            const CountCallback &rcb,
                            const ExceptionCallback &ecb) noexcept;

    /**
     * @brief Delete the record that matches the given primary key
     * asynchronously.
     *
     * @param key The primary key.
     * @return std::future<size_t> The future object with which user can get the
     * number of deleted records
     */
    std::future<size_t> deleteFutureByPrimaryKey(
        const TraitsPKType &key) noexcept;

  private:
    DbClientPtr client_;
    size_t limit_{0};
    size_t offset_{0};
    std::string orderByString_;
    bool forUpdate_{false};
    void clear()
    {
        limit_ = 0;
        offset_ = 0;
        orderByString_.clear();
        forUpdate_ = false;
    }
    template <typename PKType = decltype(T::primaryKeyName)>
    typename std::enable_if<std::is_same<const std::string, PKType>::value,
                            void>::type
    makePrimaryKeyCriteria(std::string &sql)
    {
        sql += " where ";
        sql += T::primaryKeyName;
        sql += " = $?";
    }
    template <typename PKType = decltype(T::primaryKeyName)>
    typename std::enable_if<
        std::is_same<const std::vector<std::string>, PKType>::value,
        void>::type
    makePrimaryKeyCriteria(std::string &sql)
    {
        sql += " where ";
        for (size_t i = 0; i < T::primaryKeyName.size(); ++i)
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
    typename std::enable_if<std::is_same<const std::string, PKType>::value,
                            void>::type
    outputPrimeryKeyToBinder(const TraitsPKType &pk,
                             internal::SqlBinder &binder)
    {
        binder << pk;
    }
    template <typename PKType = decltype(T::primaryKeyName)>
    typename std::enable_if<
        std::is_same<const std::vector<std::string>, PKType>::value,
        void>::type
    outputPrimeryKeyToBinder(const TraitsPKType &pk,
                             internal::SqlBinder &binder)
    {
        tupleToBinder<typename T::PrimaryKeyType>(pk, binder);
    }

    template <typename TP, ssize_t N = std::tuple_size<TP>::value>
    typename std::enable_if<(N > 1), void>::type tupleToBinder(
        const TP &t,
        internal::SqlBinder &binder)
    {
        tupleToBinder<TP, N - 1>(t, binder);
        binder << std::get<N - 1>(t);
    }
    template <typename TP, ssize_t N = std::tuple_size<TP>::value>
    typename std::enable_if<(N == 1), void>::type tupleToBinder(
        const TP &t,
        internal::SqlBinder &binder)
    {
        binder << std::get<0>(t);
    }

    std::string replaceSqlPlaceHolder(const std::string &sqlStr,
                                      const std::string &holderStr) const;
};

template <typename T>
inline T Mapper<T>::findOne(const Criteria &criteria) noexcept(false)
{
    std::string sql = "select * from ";
    sql += T::tableName;
    bool hasParameters = false;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        hasParameters = true;
    }
    sql.append(orderByString_);
    if (limit_ > 0)
    {
        hasParameters = true;
        sql.append(" limit $?");
    }
    if (offset_ > 0)
    {
        hasParameters = true;
        sql.append(" offset $?");
    }
    if (hasParameters)
        sql = replaceSqlPlaceHolder(sql, "$?");
    if (forUpdate_)
    {
        sql += " for update";
    }
    Result r(nullptr);
    {
        auto binder = *client_ << std::move(sql);
        if (criteria)
            criteria.outputArgs(binder);
        if (limit_ > 0)
            binder << limit_;
        if (offset_)
            binder << offset_;
        clear();
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // exec may be throw exception;
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
    bool hasParameters = false;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        hasParameters = true;
    }
    sql.append(orderByString_);
    if (limit_ > 0)
    {
        hasParameters = true;
        sql.append(" limit $?");
    }
    if (offset_ > 0)
    {
        hasParameters = true;
        sql.append(" offset $?");
    }
    if (hasParameters)
        sql = replaceSqlPlaceHolder(sql, "$?");
    if (forUpdate_)
    {
        sql += " for update";
    }
    auto binder = *client_ << std::move(sql);
    if (criteria)
        criteria.outputArgs(binder);
    if (limit_ > 0)
        binder << limit_;
    if (offset_)
        binder << offset_;
    clear();
    binder >> [ecb, rcb](const Result &r) {
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
inline std::future<T> Mapper<T>::findFutureOne(
    const Criteria &criteria) noexcept
{
    std::string sql = "select * from ";
    sql += T::tableName;
    bool hasParameters = false;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        hasParameters = true;
    }
    sql.append(orderByString_);
    if (limit_ > 0)
    {
        hasParameters = true;
        sql.append(" limit $?");
    }
    if (offset_ > 0)
    {
        hasParameters = true;
        sql.append(" offset $?");
    }
    if (hasParameters)
        sql = replaceSqlPlaceHolder(sql, "$?");
    if (forUpdate_)
    {
        sql += " for update";
    }
    auto binder = *client_ << std::move(sql);
    if (criteria)
        criteria.outputArgs(binder);
    if (limit_ > 0)
        binder << limit_;
    if (offset_)
        binder << offset_;
    clear();
    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    binder >> [prom](const Result &r) {
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
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
    binder.exec();
    return prom->get_future();
}
template <typename T>
inline std::vector<T> Mapper<T>::findBy(const Criteria &criteria) noexcept(
    false)
{
    std::string sql = "select * from ";
    sql += T::tableName;
    bool hasParameters = false;
    if (criteria)
    {
        hasParameters = true;
        sql += " where ";
        sql += criteria.criteriaString();
    }
    sql.append(orderByString_);
    if (limit_ > 0)
    {
        hasParameters = true;
        sql.append(" limit $?");
    }
    if (offset_ > 0)
    {
        hasParameters = true;
        sql.append(" offset $?");
    }
    if (hasParameters)
        sql = replaceSqlPlaceHolder(sql, "$?");
    if (forUpdate_)
    {
        sql += " for update";
    }
    Result r(nullptr);
    {
        auto binder = *client_ << std::move(sql);
        if (criteria)
            criteria.outputArgs(binder);
        if (limit_ > 0)
            binder << limit_;
        if (offset_)
            binder << offset_;
        clear();
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // exec may be throw exception;
    }
    std::vector<T> ret;
    for (auto const &row : r)
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
    bool hasParameters = false;
    if (criteria)
    {
        hasParameters = true;
        sql += " where ";
        sql += criteria.criteriaString();
    }
    sql.append(orderByString_);
    if (limit_ > 0)
    {
        hasParameters = true;
        sql.append(" limit $?");
    }
    if (offset_ > 0)
    {
        hasParameters = true;
        sql.append(" offset $?");
    }
    if (hasParameters)
        sql = replaceSqlPlaceHolder(sql, "$?");
    if (forUpdate_)
    {
        sql += " for update";
    }
    auto binder = *client_ << std::move(sql);
    if (criteria)
        criteria.outputArgs(binder);
    if (limit_ > 0)
        binder << limit_;
    if (offset_)
        binder << offset_;
    clear();
    binder >> [rcb](const Result &r) {
        std::vector<T> ret;
        for (auto const &row : r)
        {
            ret.push_back(T(row));
        }
        rcb(ret);
    };
    binder >> ecb;
}
template <typename T>
inline std::future<std::vector<T>> Mapper<T>::findFutureBy(
    const Criteria &criteria) noexcept
{
    std::string sql = "select * from ";
    sql += T::tableName;
    bool hasParameters = false;
    if (criteria)
    {
        hasParameters = true;
        sql += " where ";
        sql += criteria.criteriaString();
    }
    sql.append(orderByString_);
    if (limit_ > 0)
    {
        hasParameters = true;
        sql.append(" limit $?");
    }
    if (offset_ > 0)
    {
        hasParameters = true;
        sql.append(" offset $?");
    }
    if (hasParameters)
        sql = replaceSqlPlaceHolder(sql, "$?");
    if (forUpdate_)
    {
        sql += " for update";
    }
    auto binder = *client_ << std::move(sql);
    if (criteria)
        criteria.outputArgs(binder);
    if (limit_ > 0)
        binder << limit_;
    if (offset_)
        binder << offset_;
    clear();
    std::shared_ptr<std::promise<std::vector<T>>> prom =
        std::make_shared<std::promise<std::vector<T>>>();
    binder >> [prom](const Result &r) {
        std::vector<T> ret;
        for (auto const &row : r)
        {
            ret.push_back(T(row));
        }
        prom->set_value(ret);
    };
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
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
        auto binder = *client_ << std::move(sql);
        if (criteria)
            criteria.outputArgs(binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // exec may be throw exception;
    }
    assert(r.size() == 1);
    return r[0][(Row::SizeType)0].as<size_t>();
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
    auto binder = *client_ << std::move(sql);
    if (criteria)
        criteria.outputArgs(binder);
    binder >> [rcb](const Result &r) {
        assert(r.size() == 1);
        rcb(r[0][(Row::SizeType)0].as<size_t>());
    };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::countFuture(
    const Criteria &criteria) noexcept
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
    auto binder = *client_ << std::move(sql);
    if (criteria)
        criteria.outputArgs(binder);

    std::shared_ptr<std::promise<size_t>> prom =
        std::make_shared<std::promise<size_t>>();
    binder >> [prom](const Result &r) {
        assert(r.size() == 1);
        prom->set_value(r[0][(Row::SizeType)0].as<size_t>());
    };
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
    binder.exec();
    return prom->get_future();
}
template <typename T>
inline void Mapper<T>::insert(T &obj) noexcept(false)
{
    clear();
    Result r(nullptr);
    bool needSelection = false;
    {
        auto binder = *client_ << obj.sqlForInserting(needSelection);
        obj.outputArgs(binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // Maybe throw exception;
    }
    assert(r.affectedRows() == 1);
    if (client_->type() == ClientType::PostgreSQL)
    {
        if (needSelection)
        {
            assert(r.size() == 1);
            obj = T(r[0]);
        }
    }
    else  // Mysql or Sqlite3
    {
        auto id = r.insertId();
        obj.updateId(id);
        if (needSelection)
        {
            obj = findByPrimaryKey(obj.getPrimaryKey());
        }
    }
}
template <typename T>
inline void Mapper<T>::insert(const T &obj,
                              const SingleRowCallback &rcb,
                              const ExceptionCallback &ecb) noexcept
{
    clear();
    bool needSelection = false;
    auto binder = *client_ << obj.sqlForInserting(needSelection);
    obj.outputArgs(binder);
    auto client = client_;
    binder >> [client, rcb, obj, needSelection, ecb](const Result &r) {
        assert(r.affectedRows() == 1);
        if (client->type() == ClientType::PostgreSQL)
        {
            if (needSelection)
            {
                assert(r.size() == 1);
                rcb(T(r[0]));
            }
            else
            {
                rcb(obj);
            }
        }
        else  // Mysql or Sqlite3
        {
            auto id = r.insertId();
            auto newObj = obj;
            newObj.updateId(id);
            if (needSelection)
            {
                auto tmp = Mapper<T>(client);
                tmp.findByPrimaryKey(newObj.getPrimaryKey(), rcb, ecb);
            }
            else
            {
                rcb(newObj);
            }
        }
    };
    binder >> ecb;
}
template <typename T>
inline std::future<T> Mapper<T>::insertFuture(const T &obj) noexcept
{
    clear();
    bool needSelection = false;
    auto binder = *client_ << obj.sqlForInserting(needSelection);
    obj.outputArgs(binder);

    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    auto client = client_;
    binder >> [client, prom, obj, needSelection](const Result &r) {
        assert(r.affectedRows() == 1);
        if (client->type() == ClientType::PostgreSQL)
        {
            if (needSelection)
            {
                assert(r.size() == 1);
                prom->set_value(T(r[0]));
            }
            else
            {
                prom->set_value(obj);
            }
        }
        else  // Mysql or Sqlite3
        {
            auto id = r.insertId();
            auto newObj = obj;
            newObj.updateId(id);
            if (needSelection)
            {
                auto tmp = Mapper<T>(client);
                tmp.findByPrimaryKey(
                    newObj.getPrimaryKey(),
                    [prom](T selObj) { prom->set_value(selObj); },
                    [prom](const std::exception_ptr &e) {
                        prom->set_exception(e);
                    });
            }
            else
            {
                prom->set_value(newObj);
            }
        }
    };
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
    binder.exec();
    return prom->get_future();
}
template <typename T>
inline size_t Mapper<T>::update(const T &obj) noexcept(false)
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "update ";
    sql += T::tableName;
    sql += " set ";
    for (auto const &colName : obj.updateColumns())
    {
        sql += colName;
        sql += " = $?,";
    }
    sql[sql.length() - 1] = ' ';  // Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    Result r(nullptr);
    {
        auto binder = *client_ << std::move(sql);
        obj.updateArgs(binder);
        outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // Maybe throw exception;
    }
    return r.affectedRows();
}
template <typename T>
inline void Mapper<T>::update(const T &obj,
                              const CountCallback &rcb,
                              const ExceptionCallback &ecb) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "update ";
    sql += T::tableName;
    sql += " set ";
    for (auto const &colName : obj.updateColumns())
    {
        sql += colName;
        sql += " = $?,";
    }
    sql[sql.length() - 1] = ' ';  // Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *client_ << std::move(sql);
    obj.updateArgs(binder);
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
    binder >> [rcb](const Result &r) { rcb(r.affectedRows()); };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::updateFuture(const T &obj) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "update ";
    sql += T::tableName;
    sql += " set ";
    for (auto const &colName : obj.updateColumns())
    {
        sql += colName;
        sql += " = $?,";
    }
    sql[sql.length() - 1] = ' ';  // Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *client_ << std::move(sql);
    obj.updateArgs(binder);
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);

    std::shared_ptr<std::promise<size_t>> prom =
        std::make_shared<std::promise<size_t>>();
    binder >> [prom](const Result &r) { prom->set_value(r.affectedRows()); };
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
    binder.exec();
    return prom->get_future();
}

template <typename T>
inline size_t Mapper<T>::deleteOne(const T &obj) noexcept(false)
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;

    sql += " ";  // Replace the last ','

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    Result r(nullptr);
    {
        auto binder = *client_ << std::move(sql);
        outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // Maybe throw exception;
    }
    return r.affectedRows();
}
template <typename T>
inline void Mapper<T>::deleteOne(const T &obj,
                                 const CountCallback &rcb,
                                 const ExceptionCallback &ecb) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;
    sql += " ";

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *client_ << std::move(sql);
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);
    binder >> [rcb](const Result &r) { rcb(r.affectedRows()); };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::deleteFutureOne(const T &obj) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;
    sql += " ";

    makePrimaryKeyCriteria(sql);

    sql = replaceSqlPlaceHolder(sql, "$?");
    auto binder = *client_ << std::move(sql);
    outputPrimeryKeyToBinder(obj.getPrimaryKey(), binder);

    std::shared_ptr<std::promise<size_t>> prom =
        std::make_shared<std::promise<size_t>>();
    binder >> [prom](const Result &r) { prom->set_value(r.affectedRows()); };
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
    binder.exec();
    return prom->get_future();
}

template <typename T>
inline size_t Mapper<T>::deleteBy(const Criteria &criteria) noexcept(false)
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
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
        auto binder = *client_ << std::move(sql);
        if (criteria)
        {
            criteria.outputArgs(binder);
        }
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // Maybe throw exception;
    }
    return r.affectedRows();
}
template <typename T>
inline void Mapper<T>::deleteBy(const Criteria &criteria,
                                const CountCallback &rcb,
                                const ExceptionCallback &ecb) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;

    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }

    auto binder = *client_ << std::move(sql);
    if (criteria)
    {
        criteria.outputArgs(binder);
    }
    binder >> [rcb](const Result &r) { rcb(r.affectedRows()); };
    binder >> ecb;
}
template <typename T>
inline std::future<size_t> Mapper<T>::deleteFutureBy(
    const Criteria &criteria) noexcept
{
    clear();
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    std::string sql = "delete from ";
    sql += T::tableName;
    if (criteria)
    {
        sql += " where ";
        sql += criteria.criteriaString();
        sql = replaceSqlPlaceHolder(sql, "$?");
    }
    auto binder = *client_ << std::move(sql);
    if (criteria)
    {
        criteria.outputArgs(binder);
    }

    std::shared_ptr<std::promise<size_t>> prom =
        std::make_shared<std::promise<size_t>>();
    binder >> [prom](const Result &r) { prom->set_value(r.affectedRows()); };
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
    binder.exec();
    return prom->get_future();
}

template <typename T>
inline Mapper<T> &Mapper<T>::limit(size_t limit)
{
    assert(limit > 0);
    limit_ = limit;
    return *this;
}
template <typename T>
inline Mapper<T> &Mapper<T>::offset(size_t offset)
{
    offset_ = offset;
    return *this;
}
template <typename T>
inline Mapper<T> &Mapper<T>::orderBy(const std::string &colName,
                                     const SortOrder &order)
{
    if (orderByString_.empty())
    {
        orderByString_ =
            utils::formattedString(" order by %s", colName.c_str());
        if (order == SortOrder::DESC)
        {
            orderByString_ += " desc";
        }
    }
    else
    {
        orderByString_ += ",";
        orderByString_ += colName;
        if (order == SortOrder::DESC)
        {
            orderByString_ += " desc";
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
    forUpdate_ = true;
    return *this;
}
template <typename T>
inline std::string Mapper<T>::replaceSqlPlaceHolder(
    const std::string &sqlStr,
    const std::string &holderStr) const
{
    if (client_->type() == ClientType::PostgreSQL)
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
    else if (client_->type() == ClientType::Mysql ||
             client_->type() == ClientType::Sqlite3)
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

template <typename T>
inline size_t Mapper<T>::deleteByPrimaryKey(
    const typename Mapper<T>::TraitsPKType &key) noexcept(false)
{
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    static_assert(internal::has_sqlForDeletingByPrimaryKey<T>::value,
                  "No function member named sqlForDeletingByPrimaryKey, please "
                  "make sure that the model class is generated by the latest "
                  "version of drogon_ctl");
    clear();
    Result r(nullptr);
    {
        auto binder = *client_ << T::sqlForDeletingByPrimaryKey();
        outputPrimeryKeyToBinder(key, binder);
        binder << Mode::Blocking;
        binder >> [&r](const Result &result) { r = result; };
        binder.exec();  // exec may be throw exception;
    }
    return r.affectedRows();
}

template <typename T>
inline void Mapper<T>::deleteByPrimaryKey(
    const typename Mapper<T>::TraitsPKType &key,
    const CountCallback &rcb,
    const ExceptionCallback &ecb) noexcept
{
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    static_assert(internal::has_sqlForDeletingByPrimaryKey<T>::value,
                  "No function member named sqlForDeletingByPrimaryKey, please "
                  "make sure that the model class is generated by the latest "
                  "version of drogon_ctl");
    clear();
    auto binder = *client_ << T::sqlForDeletingByPrimaryKey();
    outputPrimeryKeyToBinder(key, binder);
    binder >>
        [rcb = std::move(rcb)](const Result &r) { rcb(r.affectedRows()); };
    binder >> ecb;
}

template <typename T>
inline std::future<size_t> Mapper<T>::deleteFutureByPrimaryKey(
    const typename Mapper<T>::TraitsPKType &key) noexcept
{
    static_assert(!std::is_same<typename T::PrimaryKeyType, void>::value,
                  "No primary key in the table!");
    static_assert(internal::has_sqlForDeletingByPrimaryKey<T>::value,
                  "No function member named sqlForDeletingByPrimaryKey, please "
                  "make sure that the model class is generated by the latest "
                  "version of drogon_ctl");
    clear();
    auto binder = *client_ << T::sqlForDeletingByPrimaryKey();
    outputPrimeryKeyToBinder(key, binder);

    std::shared_ptr<std::promise<T>> prom = std::make_shared<std::promise<T>>();
    binder >> [prom](const Result &r) { prom->set_value(r.affectedRows()); };
    binder >> [prom](const std::exception_ptr &e) { prom->set_exception(e); };
    binder.exec();
    return prom->get_future();
}

}  // namespace orm
}  // namespace drogon
