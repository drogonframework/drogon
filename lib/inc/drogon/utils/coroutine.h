/**
 *
 *  coroutine.h
 *  Martin Chang
 *
 *  Copyright 2021, Martin Chang.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <drogon/utils/optional.h>
#include <trantor/net/EventLoop.h>
#include <trantor/utils/Logger.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <future>
#include <mutex>
#include <type_traits>

namespace drogon
{
namespace internal
{
template <typename T>
auto getAwaiterImpl(T &&value) noexcept(
    noexcept(static_cast<T &&>(value).operator co_await()))
    -> decltype(static_cast<T &&>(value).operator co_await())
{
    return static_cast<T &&>(value).operator co_await();
}

template <typename T>
auto getAwaiterImpl(T &&value) noexcept(
    noexcept(operator co_await(static_cast<T &&>(value))))
    -> decltype(operator co_await(static_cast<T &&>(value)))
{
    return operator co_await(static_cast<T &&>(value));
}

template <typename T>
auto getAwaiter(T &&value) noexcept(
    noexcept(getAwaiterImpl(static_cast<T &&>(value))))
    -> decltype(getAwaiterImpl(static_cast<T &&>(value)))
{
    return getAwaiterImpl(static_cast<T &&>(value));
}

}  // end namespace internal

template <typename T>
struct await_result
{
    using awaiter_t = decltype(internal::getAwaiter(std::declval<T>()));
    using type = decltype(std::declval<awaiter_t>().await_resume());
};

template <typename T>
using await_result_t = typename await_result<T>::type;

template <typename T, typename = std::void_t<>>
struct is_awaitable : std::false_type
{
};

template <typename T>
struct is_awaitable<
    T,
    std::void_t<decltype(internal::getAwaiter(std::declval<T>()))>>
    : std::true_type
{
};

template <typename T>
constexpr bool is_awaitable_v = is_awaitable<T>::value;

struct final_awaiter
{
    bool await_ready() noexcept
    {
        return false;
    }
    template <typename T>
    auto await_suspend(std::coroutine_handle<T> handle) noexcept
    {
        return handle.promise().continuation_;
    }
    void await_resume() noexcept
    {
    }
};

template <typename T = void>
struct [[nodiscard]] Task
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : coro_(h)
    {
    }
    Task(const Task &) = delete;
    Task(Task &&other)
    {
        coro_ = other.coro_;
        other.coro_ = nullptr;
    }
    ~Task()
    {
        if (coro_)
            coro_.destroy();
    }
    Task &operator=(const Task &) = delete;
    Task &operator=(Task &&other)
    {
        if (std::addressof(other) == this)
            return *this;
        if (coro_)
            coro_.destroy();

        coro_ = other.coro_;
        other.coro_ = nullptr;
        return *this;
    }

    struct promise_type
    {
        Task<T> get_return_object()
        {
            return Task<T>{handle_type::from_promise(*this)};
        }
        std::suspend_always initial_suspend()
        {
            return {};
        }
        void return_value(const T &v)
        {
            value = v;
        }

        auto final_suspend() noexcept
        {
            return final_awaiter{};
        }

        void unhandled_exception()
        {
            exception_ = std::current_exception();
        }

        T &&result() &&
        {
            if (exception_ != nullptr)
                std::rethrow_exception(exception_);
            assert(value.has_value() == true);
            return std::move(value.value());
        }

        T &result() &
        {
            if (exception_ != nullptr)
                std::rethrow_exception(exception_);
            assert(value.has_value() == true);
            return value.value();
        }

        void setContinuation(std::coroutine_handle<> handle)
        {
            continuation_ = handle;
        }

        optional<T> value;
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;
    };
    bool await_ready() const
    {
        return !coro_ || coro_.done();
    }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting)
    {
        coro_.promise().setContinuation(awaiting);
        return coro_;
    }

    auto operator co_await() const &noexcept
    {
        struct awaiter
        {
          public:
            explicit awaiter(handle_type coro) : coro_(coro)
            {
            }
            bool await_ready() noexcept
            {
                return !coro_ || coro_.done();
            }
            auto await_suspend(std::coroutine_handle<> handle) noexcept
            {
                coro_.promise().setContinuation(handle);
                return coro_;
            }
            T await_resume()
            {
                auto &&v = coro_.promise().result();
                return v;
            }

          private:
            handle_type coro_;
        };
        return awaiter(coro_);
    }

    auto operator co_await() const &&noexcept
    {
        struct awaiter
        {
          public:
            explicit awaiter(handle_type coro) : coro_(coro)
            {
            }
            bool await_ready() noexcept
            {
                return !coro_ || coro_.done();
            }
            auto await_suspend(std::coroutine_handle<> handle) noexcept
            {
                coro_.promise().setContinuation(handle);
                return coro_;
            }
            T await_resume()
            {
                return std::move(coro_.promise().result());
            }

          private:
            handle_type coro_;
        };
        return awaiter(coro_);
    }
    handle_type coro_;
};

template <>
struct [[nodiscard]] Task<void>
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type handle) : coro_(handle)
    {
    }
    Task(const Task &) = delete;
    Task(Task &&other)
    {
        coro_ = other.coro_;
        other.coro_ = nullptr;
    }
    ~Task()
    {
        if (coro_)
            coro_.destroy();
    }
    Task &operator=(const Task &) = delete;
    Task &operator=(Task &&other)
    {
        if (std::addressof(other) == this)
            return *this;
        if (coro_)
            coro_.destroy();

        coro_ = other.coro_;
        other.coro_ = nullptr;
        return *this;
    }

    struct promise_type
    {
        Task<> get_return_object()
        {
            return Task<>{handle_type::from_promise(*this)};
        }
        std::suspend_always initial_suspend()
        {
            return {};
        }
        void return_void()
        {
        }
        auto final_suspend() noexcept
        {
            return final_awaiter{};
        }
        void unhandled_exception()
        {
            exception_ = std::current_exception();
        }
        void result()
        {
            if (exception_ != nullptr)
                std::rethrow_exception(exception_);
        }
        void setContinuation(std::coroutine_handle<> handle)
        {
            continuation_ = handle;
        }
        std::exception_ptr exception_;
        std::coroutine_handle<> continuation_;
    };
    bool await_ready()
    {
        return coro_.done();
    }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting)
    {
        coro_.promise().setContinuation(awaiting);
        return coro_;
    }
    auto operator co_await() const &noexcept
    {
        struct awaiter
        {
          public:
            explicit awaiter(handle_type coro) : coro_(coro)
            {
            }
            bool await_ready() noexcept
            {
                return !coro_ || coro_.done();
            }
            auto await_suspend(std::coroutine_handle<> handle) noexcept
            {
                coro_.promise().setContinuation(handle);
                return coro_;
            }
            auto await_resume()
            {
                coro_.promise().result();
            }

          private:
            handle_type coro_;
        };
        return awaiter(coro_);
    }

    auto operator co_await() const &&noexcept
    {
        struct awaiter
        {
          public:
            explicit awaiter(handle_type coro) : coro_(coro)
            {
            }
            bool await_ready() noexcept
            {
                return false;
            }
            auto await_suspend(std::coroutine_handle<> handle) noexcept
            {
                coro_.promise().setContinuation(handle);
                return coro_;
            }
            void await_resume()
            {
                coro_.promise().result();
            }

          private:
            handle_type coro_;
        };
        return awaiter(coro_);
    }
    handle_type coro_;
};

/// Fires a coroutine and doesn't force waiting nor deallocates upon promise
/// destructs
// NOTE: AsyncTask is designed to be not awaitable. And kills the entire process
// if exception escaped.
struct AsyncTask
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    AsyncTask(handle_type h) : coro_(h)
    {
    }
    AsyncTask(const AsyncTask &) = delete;

    ~AsyncTask()
    {
        if (coro_)
            coro_.destroy();
    }
    AsyncTask &operator=(const AsyncTask &) = delete;
    AsyncTask &operator=(AsyncTask &&other)
    {
        if (std::addressof(other) == this)
            return *this;
        if (coro_)
            coro_.destroy();

        coro_ = other.coro_;
        other.coro_ = nullptr;
        return *this;
    }

    struct promise_type
    {
        std::coroutine_handle<> continuation_;

        AsyncTask get_return_object() noexcept
        {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() const noexcept
        {
            return {};
        }

        void unhandled_exception()
        {
            LOG_FATAL << "Unhandled exception in AsyncTask.";
            abort();
        }

        void return_void() noexcept
        {
        }

        void setContinuation(std::coroutine_handle<> handle)
        {
            continuation_ = handle;
        }

        auto final_suspend() const noexcept
        {
            struct awaiter
            {
                bool await_ready() const noexcept
                {
                    return false;
                }

                void await_resume() const noexcept
                {
                }

                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> handle) noexcept
                {
                    auto coro = handle.promise().continuation_;
                    if (coro)
                        return coro;

                    return std::noop_coroutine();
                }
            };
            return awaiter{};
        }
    };
    bool await_ready() const noexcept
    {
        return coro_.done();
    }

    void await_resume() const noexcept
    {
    }

    void await_suspend(std::coroutine_handle<> coroutine) const noexcept
    {
        coro_.promise().setContinuation(coroutine);
    }

    handle_type coro_;
};

/// Helper class that provides the infrastructure for turning callback into
/// coroutines
// The user is responsible to fill in `await_suspend()` and constructors.
template <typename T = void>
struct CallbackAwaiter
{
    bool await_ready() noexcept
    {
        return false;
    }

    const T &await_resume() const noexcept(false)
    {
        // await_resume() should always be called after co_await
        // (await_suspend()) is called. Therefore the value should always be set
        // (or there's an exception)
        assert(result_.has_value() == true || exception_ != nullptr);

        if (exception_)
            std::rethrow_exception(exception_);
        return result_.value();
    }

  private:
    // HACK: Not all desired types are default constructable. But we need the
    // entire struct to be constructed for awaiting. std::optional takes care of
    // that.
    optional<T> result_;
    std::exception_ptr exception_{nullptr};

  protected:
    void setException(const std::exception_ptr &e)
    {
        exception_ = e;
    }
    void setValue(const T &v)
    {
        result_.emplace(v);
    }
    void setValue(T &&v)
    {
        result_.emplace(std::move(v));
    }
};

template <>
struct CallbackAwaiter<void>
{
    bool await_ready() noexcept
    {
        return false;
    }

    void await_resume() noexcept(false)
    {
        if (exception_)
            std::rethrow_exception(exception_);
    }

  private:
    std::exception_ptr exception_{nullptr};

  protected:
    void setException(const std::exception_ptr &e)
    {
        exception_ = e;
    }
};

// An ok implementation of sync_await. This allows one to call
// coroutines and wait for the result from a function.
template <typename Await>
auto sync_wait(Await &&await)
{
    static_assert(is_awaitable_v<std::decay_t<Await>>);
    using value_type = typename await_result<Await>::type;
    std::condition_variable cv;
    std::mutex mtx;
    std::atomic<bool> flag = false;
    std::exception_ptr exception_ptr;
    std::unique_lock lk(mtx);

    if constexpr (std::is_same_v<value_type, void>)
    {
        auto task = [&]() -> AsyncTask {
            try
            {
                co_await await;
            }
            catch (...)
            {
                exception_ptr = std::current_exception();
            }
            std::unique_lock lk(mtx);
            flag = true;
            cv.notify_all();
        };

        // HACK: Workarround coroutine frame destructing too early by enforcing
        //       manual lifetime
        AsyncTask *taskPtr;
        std::thread thr([&]() { taskPtr = new AsyncTask{task()}; });
        cv.wait(lk, [&]() { return (bool)flag; });
        thr.join();
        delete taskPtr;
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
    }
    else
    {
        optional<value_type> value;
        auto task = [&]() -> AsyncTask {
            try
            {
                value = co_await await;
            }
            catch (...)
            {
                exception_ptr = std::current_exception();
            }
            std::unique_lock lk(mtx);
            flag = true;
            cv.notify_all();
        };

        // HACK: Workarround coroutine frame destructing too early by enforcing
        //       manual lifetime
        AsyncTask *taskPtr;
        std::thread thr([&]() { taskPtr = new AsyncTask{task()}; });
        cv.wait(lk, [&]() { return (bool)flag; });
        assert(value.has_value() == true || exception_ptr);
        thr.join();
        delete taskPtr;
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
        return value.value();
    }
}

// Converts a task (or task like) promise into std::future for old-style async
template <typename Await>
inline auto co_future(Await await) noexcept
    -> std::future<await_result_t<Await>>
{
    using Result = await_result_t<Await>;
    std::promise<Result> prom;
    auto fut = prom.get_future();
    std::promise<AsyncTask *> selfProm;
    auto selfFut = selfProm.get_future();
    auto task = [](std::promise<Result> prom,
                   Await await,
                   std::future<AsyncTask *> selfFut) -> AsyncTask {
        try
        {
            if constexpr (std::is_void_v<Result>)
            {
                co_await std::move(await);
                prom.set_value();
            }
            else
                prom.set_value(co_await std::move(await));
        }
        catch (...)
        {
            prom.set_exception(std::current_exception());
        }

        AsyncTask *self = selfFut.get();
        delete self;
    };
    AsyncTask *taskPtr = new AsyncTask{
        task(std::move(prom), std::move(await), std::move(selfFut))};
    selfProm.set_value(taskPtr);
    return fut;
}

namespace internal
{
struct TimerAwaiter : CallbackAwaiter<void>
{
    TimerAwaiter(trantor::EventLoop *loop,
                 const std::chrono::duration<double> &delay)
        : loop_(loop), delay_(delay.count())
    {
    }
    TimerAwaiter(trantor::EventLoop *loop, double delay)
        : loop_(loop), delay_(delay)
    {
    }
    void await_suspend(std::coroutine_handle<> handle)
    {
        loop_->runAfter(delay_, [handle]() { handle.resume(); });
    }

  private:
    trantor::EventLoop *loop_;
    double delay_;
};
}  // namespace internal

inline internal::TimerAwaiter sleepCoro(
    trantor::EventLoop *loop,
    const std::chrono::duration<double> &delay) noexcept
{
    assert(loop);
    return internal::TimerAwaiter(loop, delay);
}

inline internal::TimerAwaiter sleepCoro(trantor::EventLoop *loop,
                                        double delay) noexcept
{
    assert(loop);
    return internal::TimerAwaiter(loop, delay);
}

template <typename T, typename = std::void_t<>>
struct is_resumable : std::false_type
{
};

template <typename T>
struct is_resumable<
    T,
    std::void_t<decltype(internal::getAwaiter(std::declval<T>()))>>
    : std::true_type
{
};

template <>
struct is_resumable<AsyncTask, std::void_t<AsyncTask>> : std::true_type
{
};

template <typename T>
constexpr bool is_resumable_v = is_resumable<T>::value;

}  // namespace drogon
