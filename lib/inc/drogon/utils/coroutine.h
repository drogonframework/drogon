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
#include <cppcoro/task.hpp>
#include <cppcoro/awaitable_traits.hpp>

namespace drogon
{
template <typename T>
using task = cppcoro::task<T>;

/// Fires a coroutine and doesn't force waiting nor deallocates upon promise
/// destructs
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

        void unhandled_exception() noexcept(false)
        {
            exception_ = std::current_exception();
        }

        promise_type* get_return_object() noexcept
        {
            return this;
        }

        void result()
        {
            if (exception_ != nullptr)
                std::rethrow_exception(exception_);
        }

      protected:
        std::exception_ptr exception_ = nullptr;
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
    // HACK: Not all desired value will default contructable. But we need the
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
