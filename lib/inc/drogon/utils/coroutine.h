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
#include <algorithm>
#include <coroutine>
#include <exception>
#include <type_traits>
#include <condition_variable>
#include <atomic>
#include <future>
#include <cassert>

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
    Task(Task && other)
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
    Task(Task && other)
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
struct AsyncTask final
{
    struct promise_type final
    {
        auto initial_suspend() noexcept
        {
            return std::suspend_never{};
        }

        auto final_suspend() noexcept
        {
            return std::suspend_never{};
        }

        void return_void() noexcept
        {
        }

        void unhandled_exception()
        {
            std::terminate();
        }

        promise_type *get_return_object() noexcept
        {
            return this;
        }

        void result()
        {
        }
    };
    AsyncTask(const promise_type *) noexcept
    {
        // the type truncates all given info about its frame
    }
};

/// Helper class that provices the infrastructure for turning callback into
/// corourines
// The user is responsible to fill in `await_suspend()` and construtors.
template <typename T = void>
struct CallbackAwaiter
{
    bool await_ready() noexcept
    {
        return false;
    }

    const T &await_resume() noexcept(false)
    {
        // await_resume() should always be called after co_await
        // (await_suspend()) is called. Therefor the value should always be set
        // (or there's an exception)
        assert(result_.has_value() == true || exception_ != nullptr);

        if (exception_)
            std::rethrow_exception(exception_);
        return result_.value();
    }

  private:
    // HACK: Not all desired types are default contructable. But we need the
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
//
// NOTE: Not sure if this is a compiler bug. But causes use after free in some
// cases. Don't use it in production code.
template <typename AWAIT>
auto sync_wait(AWAIT &&await)
{
    using value_type = typename await_result<AWAIT>::type;
    std::condition_variable cv;
    std::mutex mtx;
    std::atomic<bool> flag = false;
    std::exception_ptr exception_ptr;

    if constexpr (std::is_same_v<value_type, void>)
    {
        [&, lk = std::unique_lock(mtx)]() -> AsyncTask {
            try
            {
                co_await await;
            }
            catch (...)
            {
                exception_ptr = std::current_exception();
            }

            flag = true;
            cv.notify_one();
        }();

        std::unique_lock lk(mtx);
        cv.wait(lk, [&]() { return (bool)flag; });
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
    }
    else
    {
        optional<value_type> value;
        [&, lk = std::unique_lock(mtx)]() -> AsyncTask {
            try
            {
                value = co_await await;
            }
            catch (const std::exception &e)
            {
                exception_ptr = std::current_exception();
            }
            flag = true;
        }();

        std::unique_lock lk(mtx);
        cv.wait(lk, [&]() { return (bool)flag; });

        assert(value.has_value() == true || exception_ptr);
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
        return value.value();
    }
}

// Converts a task (or task like) promise into std::future for old-style async
// NOTE: Not sure if this is a compiler bug. But causes use after free in some
// cases. Don't use it in production code.
template <typename Await>
inline auto co_future(Await await) noexcept
    -> std::future<await_result_t<Await>>
{
    using Result = await_result_t<Await>;
    std::promise<Result> prom;
    auto fut = prom.get_future();
    [](std::promise<Result> &&prom, Await &&await) -> AsyncTask {
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
    }(std::move(prom), std::move(await));
    return fut;
}

namespace internal
{
struct TimerAwaiter : CallbackAwaiter<void>
{
    TimerAwaiter(trantor::EventLoop *loop,
                 const std::chrono::duration<long double> &delay)
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

inline Task<void> sleepCoro(
    trantor::EventLoop *loop,
    const std::chrono::duration<long double> &delay) noexcept
{
    assert(loop);
    co_return co_await internal::TimerAwaiter(loop, delay);
}

inline Task<void> sleepCoro(trantor::EventLoop *loop, double delay) noexcept
{
    assert(loop);
    co_return co_await internal::TimerAwaiter(loop, delay);
}

}  // namespace drogon
