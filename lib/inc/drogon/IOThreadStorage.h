/**
 *
 *  IOThreadStorage.h
 *  Daniel Mensinger
 *
 *  Copyright 2019, Daniel Mensinger.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <memory>
#include <vector>
#include <limits>
#include <functional>

#include <drogon/HttpAppFramework.h>

namespace drogon
{
/**
 * @brief Utility class for thread storage handling
 *
 * Thread storage allows the efficient handling of reusable data without thread
 * synchronisation. For instance, such a thread storage would be useful to store
 * database connections.
 *
 * Example usage:
 *
 * @code
 * struct MyThreadData {
 *     int threadLocal = 42;
 *     std::string something = "foo";
 * };
 *
 * class MyController : public HttpController<MyController> {
 *   public:
 *      METHOD_LIST_BEGIN
 *      ADD_METHOD_TO(MyController::endpoint, "/some/path", Get);
 *      METHOD_LIST_END
 *
 *      void login(const HttpRequestPtr &req,
 *                 std::function<void (const HttpResponsePtr &)> &&callback) {
 *          assert(_storage->threadLocal == 42);
 *
 *          // handle the request
 *      }
 *
 *    private:
 *      IOThreadStorage<MyThreadData> _storage;
 * };
 * @endcode
 */
template <class C,
          bool AutoInit = true,
          template <class> class StoragePtrType = std::shared_ptr>
class IOThreadStorage
{
  private:
    using IsSharedPtr = std::is_same<StoragePtrType<C>, std::shared_ptr<C>>;

    static_assert(!AutoInit || (AutoInit && IsSharedPtr::value),
                  "Default initialization only works with std::shared_ptr");

  public:
    static const bool isConstructorInitialized = AutoInit;
    using StoragePtr = StoragePtrType<C>;
    using CreatorCallback = std::function<StoragePtr(size_t idx)>;

    template <typename U = C,
              typename = typename std::enable_if<
                  std::is_default_constructible<U>::value &&
                  IsSharedPtr::value && AutoInit>::type>
    IOThreadStorage()
        : IOThreadStorage([](size_t) { return std::make_shared<C>(); })
    {
    }

    template <typename U = C,
              typename = typename std::enable_if<!AutoInit, U>::type,
              typename = U>
    IOThreadStorage() : IOThreadStorage([](size_t) { return nullptr; })
    {
    }

    IOThreadStorage(const CreatorCallback &creator)
    {
        size_t numThreads = app().getThreadNum();
        assert(numThreads > 0 &&
               numThreads != std::numeric_limits<size_t>::max());
        _storage.resize(numThreads);

        for (size_t i = 0; i < numThreads; ++i)
        {
            _storage[i] = creator(i);
        }
    }

    /**
     * @brief Get the thread storage asociate with the current thread
     *
     * This function may only be called in a request handler
     */
    inline StoragePtr &getThreadData()
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        return _storage[idx];
    }

    /**
     * @brief Sets the thread data for the current thread
     *
     * This function may only be called in a request handler
     */
    inline void setThreadData(const StoragePtr &newData)
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        _storage[idx] = newData;
    }

    inline void setThreadData(StoragePtr &&newData)
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        _storage[idx] = std::move(newData);
    }

    inline C *operator->()
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        return _storage[idx].get();
    }

    inline StoragePtr &operator*()
    {
        return getThreadData();
    }

    inline explicit operator bool() const
    {
        return (bool)getThreadData();
    }

  private:
    std::vector<StoragePtr> _storage;
};

}  // namespace drogon
