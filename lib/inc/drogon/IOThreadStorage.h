/**
 *
 *  @file IOThreadStorage.h
 *  @author Daniel Mensinger
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

#include <drogon/HttpAppFramework.h>
#include <trantor/utils/NonCopyable.h>
#include <memory>
#include <vector>
#include <limits>
#include <functional>

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
 *          assert(storage_->threadLocal == 42);
 *
 *          // handle the request
 *      }
 *
 *    private:
 *      IOThreadStorage<MyThreadData> storage_;
 * };
 * @endcode
 */
template <typename C>
class IOThreadStorage : public trantor::NonCopyable
{
  public:
    using ValueType = C;
    using InitCallback = std::function<void(ValueType &, size_t)>;

    template <typename... Args>
    IOThreadStorage(Args &&...args)
    {
        static_assert(std::is_constructible<C, Args &&...>::value,
                      "Unable to construct storage with given signature");
        size_t numThreads = app().getThreadNum();
        assert(numThreads > 0 &&
               numThreads != (std::numeric_limits<size_t>::max)());
        // set the size to numThreads+1 to enable access to this in the main
        // thread.
        storage_.reserve(numThreads + 1);

        for (size_t i = 0; i <= numThreads; ++i)
        {
            storage_.emplace_back(std::forward<Args>(args)...);
        }
    }

    void init(const InitCallback &initCB)
    {
        for (size_t i = 0; i < storage_.size(); ++i)
        {
            initCB(storage_[i], i);
        }
    }

    /**
     * @brief Get the thread storage associate with the current thread
     *
     * This function may only be called in a request handler
     */
    inline ValueType &getThreadData()
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < storage_.size());
        return storage_[idx];
    }

    inline const ValueType &getThreadData() const
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < storage_.size());
        return storage_[idx];
    }

    /**
     * @brief Sets the thread data for the current thread
     *
     * This function may only be called in a request handler
     */
    inline void setThreadData(const ValueType &newData)
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < storage_.size());
        storage_[idx] = newData;
    }

    inline void setThreadData(ValueType &&newData)
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < storage_.size());
        storage_[idx] = std::move(newData);
    }

    inline ValueType *operator->()
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < storage_.size());
        return &storage_[idx];
    }

    inline ValueType &operator*()
    {
        return getThreadData();
    }

    inline const ValueType *operator->() const
    {
        size_t idx = app().getCurrentThreadIndex();
        assert(idx < storage_.size());
        return &storage_[idx];
    }

    inline const ValueType &operator*() const
    {
        return getThreadData();
    }

  private:
    std::vector<ValueType> storage_;
};

inline trantor::EventLoop *getIOThreadStorageLoop(size_t index) noexcept(false)
{
    if (index > drogon::app().getThreadNum())
    {
        throw std::out_of_range("Event loop index is out of range");
    }
    if (index == drogon::app().getThreadNum())
        return drogon::app().getLoop();
    return drogon::app().getIOLoop(index);
}
}  // namespace drogon
