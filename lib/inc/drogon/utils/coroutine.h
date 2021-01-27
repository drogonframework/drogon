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

#include <coroutine>
#include <exception>
#include <drogon/utils/optional.h>
#include <type_traits>

namespace drogon
{

template <typename T>
struct final_awiter {
    bool await_ready() noexcept { return false; }
    auto await_suspend(std::coroutine_handle<T> handle) noexcept {
        return handle.promise().continuation_;
    }
    void await_resume() noexcept {}
};

template <typename T=void>
struct Task
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h)
        : coro_(h)
    {}
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
        void return_value(const T& v)
        {
            value = v;
        }

        auto final_suspend() noexcept {
            return final_awiter<promise_type>{};
        }

        void unhandled_exception()
        {
            exception_ = std::current_exception();
        }
        const T& result() const
        {
          if(exception_ != nullptr)
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
        return coro_.done();
    }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting)
    {
        coro_.promise().setContinuation(awaiting);
        return coro_;
    }

    auto operator co_await() const noexcept {
        struct awaiter {
        public:
            explicit awaiter(handle_type coro) : coro_(coro) {}
            bool await_ready() noexcept {
                return false;
            }
            auto await_suspend(std::coroutine_handle<> handle) noexcept {
                coro_.promise().setContinuation(handle);
                return coro_;
            }
            T await_resume() {
                return coro_.promise().result();
            }
        private:
            handle_type coro_;
        };
        return awaiter(coro_);
    }
    handle_type coro_;
};


template <>
struct Task<void>
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type handle)
        : coro_(handle)
    {}
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
        void return_value()
        {
        }
        auto final_suspend() noexcept {
            return final_awiter<promise_type>{};
        }
        void unhandled_exception()
        {
           exception_ = std::current_exception(); 
        }
        void result()
        {
            if(exception_ != nullptr)
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
    auto operator co_await() const noexcept {
        struct awaiter {
        public:
            explicit awaiter(handle_type coro) : coro_(coro) {}
            bool await_ready() noexcept {
                return false;
            }
            auto await_suspend(std::coroutine_handle<> handle) noexcept {
                coro_.promise().setContinuation(handle);
                return coro_;
            }
            void await_resume() {
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

        promise_type* get_return_object() noexcept
        {
            return this;
        }

        void result()
        {
        }
    };
    AsyncTask(const promise_type*) noexcept
    {
        // the type truncates all given info about its frame
    }
};

/// Helper class that provices the infrastructure for turning callback into
/// corourines
// The user is responsible to fill in `await_suspend()` and construtors.
template <typename T>
struct CallbackAwaiter
{
    bool await_ready() noexcept
    {
        return false;
    }

    const T& await_resume() noexcept(false)
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
    std::exception_ptr exception_ = nullptr;

  protected:
    void setException(const std::exception_ptr& e)
    {
        exception_ = e;
    }
    void setValue(const T& v)
    {
        result_.emplace(v);
    }
};

}  // namespace drogon
