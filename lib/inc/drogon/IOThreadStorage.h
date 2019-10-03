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
template <typename C, typename... Args>
class IOThreadStorage
{
    static_assert(std::is_constructible<C, Args &&...>::value,
                  "Unable to construct storage with given signature");

  public:
    using InitCallback = std::function<void(C &, size_t)>;

    IOThreadStorage(Args &&... args)
        : IOThreadStorage(std::forward(args)..., [](C &, size_t) {})
    {
    }

    IOThreadStorage(Args &&... args, const InitCallback &initCB)
    {
        size_t numThreads = app().getThreadNum();
        assert(numThreads > 0 &&
               numThreads != std::numeric_limits<size_t>::max());
        // set the size to numThreads+1 to enable access to this in the main
        // thread.
        _storage.reserve(numThreads + 1);

        for (size_t i = 0; i <= numThreads; ++i)
        {
            _storage.emplace_back(std::forward(args)...);
            initCB(_storage[i], i);
        }
    }

    /**
     * @brief Get the thread storage asociate with the current thread
     *
     * This function may only be called in a request handler
     */
    inline C &getThreadData()
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        return _storage[idx];
    }

    inline const C &getThreadData() const
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
    inline void setThreadData(const C &newData)
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        _storage[idx] = newData;
    }

    inline void setThreadData(C &&newData)
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        _storage[idx] = std::move(newData);
    }

    inline C *operator->()
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < _storage.size());
        return &_storage[idx];
    }

    inline C &operator*()
    {
        return getThreadData();
    }

    inline const C &operator*() const
    {
        return getThreadData();
    }

  private:
    std::vector<C> _storage;
};

}  // namespace drogon
