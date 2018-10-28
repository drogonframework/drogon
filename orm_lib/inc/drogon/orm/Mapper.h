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

namespace drogon
{
namespace orm
{
class Condition;
template <typename T>
class Mapper
{
  public:
    typedef std::function<void(T)> SingleRowCallback;
    typedef std::function<void(std::vector<T>)> MultipleRowsCallback;
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

    //find one
    //find by conditions
    //insert
    //update
    //update all
    //count
    //limit/offset/orderby
    //...
  private:
    Dbclient &_client;
    std::string getSqlForFindingByPrimaryKey();
};

} // namespace orm
} // namespace drogon
