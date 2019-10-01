/**
 *
 *  ThreadStorage.h
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

#include <drogon/HttpAppFramework.h>

namespace drogon
{
/**
 * @brief Base class for all thread storage containers
 */
class ThreadDataBase
{
  public:
    // Requierd for std::dynamic_pointer_cast
    virtual ~ThreadDataBase()
    {
    }
};

using ThreadDataPtr = std::shared_ptr<ThreadDataBase>;

/**
 * @brief Abstract Utility class for thread storage handling
 *
 * Thread storage allows the efficient handling of reusable data without thread
 * synchronisation. For instance, such a thread storage would be useful to store
 * database connections.
 *
 * @note initializeThreadStorage() MUST be called in the constructor of the
 * derived class
 *
 * Example usage:
 *
 * @code
 * class MyThreadData : public ThreadDataBase {
 *   public:
 *     int threadLocal;
 * };
 *
 * class MyController : public HttpController<MyController>,
 *                      public ThreadStorage {
 *   public:
 *      METHOD_LIST_BEGIN
 *      ADD_METHOD_TO(MyController::endpoint, "/some/path", Get);
 *      METHOD_LIST_END
 *
 *      MyController()
 *      {
 *          initThreadStorage();
 *      }
 *
 *      ThreadDataPtr createDataContainer(size_t thread) override
 *      {
 *          return std::make_shared<MyThreadData>();
 *      }
 *
 *      void login(const HttpRequestPtr &req,
 *                 std::function<void (const HttpResponsePtr &)> &&callback) {
 *          auto data = getThreadData<MyThreadData>();
 *
 *          // handle the request
 *      }
 * };
 * @endcode
 */
class ThreadStorage
{
  public:
    virtual ~ThreadStorage()
    {
    }

    virtual ThreadDataPtr createDataContainer(size_t thread) = 0;

    inline bool initialized() const noexcept
    {
        return !_storage.empty();
    }

    /**
     * @brief Initializes the thread storage
     *
     * Every thread is assigned a data storage container returned from
     * createDataContainer
     *
     * @note This function MUST be called in the constructor of the derived
     * class.
     */
    void initThreadStorage()
    {
        if (initialized())
        {
            return;
        }

        size_t numThreads = app().getThreadNum();
        assert(numThreads > 0 &&
               numThreads != std::numeric_limits<size_t>::max());

        _storage.resize(numThreads);
        for (size_t i = 0; i < numThreads; ++i)
        {
            _storage[i] = createDataContainer(i);
        }
    }

    /**
     * @brief Get the thread storage asociate with the current thread
     *
     * This function may only be called in a request handler
     */
    inline ThreadDataPtr getThreadData()
    {
        if (!initialized())
        {
            return nullptr;
        }

        size_t idx = app().getCurrentThreadIndex();
        if (idx >= _storage.size())
        {
            return nullptr;
        }

        return _storage[idx];
    }

    template <class T>
    inline std::shared_ptr<T> getThreadData()
    {
        return std::dynamic_pointer_cast<T>(getThreadData());
    }

  private:
    std::vector<ThreadDataPtr> _storage;
};

}  // namespace drogon
