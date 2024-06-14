//
// Created by wanchen.he on 2023/12/29.
//
#ifdef __cpp_impl_coroutine
#include <drogon/utils/coroutine.h>

namespace drogon
{
class CancelHandleImpl : public CancelHandle
{
  public:
    virtual void setCancelHandle(std::function<void()> handle) = 0;
};

class SimpleCancelHandleImpl : public CancelHandleImpl
{
  public:
    SimpleCancelHandleImpl() = default;

    void cancel() override
    {
        std::function<void()> handle;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelRequested_ = true;
            handle = std::move(cancelHandle_);
        }
        if (handle)
            handle();
    }

    bool isCancelRequested() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return cancelRequested_;
    }

    void setCancelHandle(std::function<void()> handle) override
    {
        bool cancelled{false};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (cancelRequested_)
            {
                cancelled = true;
            }
            else
            {
                cancelHandle_ = std::move(handle);
            }
        }
        if (cancelled)
        {
            handle();
        }
    }

  private:
    std::mutex mutex_;
    bool cancelRequested_{false};
    std::function<void()> cancelHandle_;
};

class SharedCancelHandleImpl : public CancelHandleImpl
{
  public:
    SharedCancelHandleImpl() = default;

    void cancel() override
    {
        std::vector<std::function<void()>> handles;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelRequested_ = true;
            handles.swap(cancelHandles_);
        }
        if (!handles.empty())
        {
            for (auto &handle : handles)
            {
                handle();
            }
        }
    }

    bool isCancelRequested() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return cancelRequested_;
    }

    void setCancelHandle(std::function<void()> handle) override
    {
        bool cancelled{false};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (cancelRequested_)
            {
                cancelled = true;
            }
            else
            {
                cancelHandles_.emplace_back(std::move(handle));
            }
        }
        if (cancelled)
        {
            handle();
        }
    }

  private:
    std::mutex mutex_;
    bool cancelRequested_{false};
    std::vector<std::function<void()>> cancelHandles_;
};

CancelHandlePtr CancelHandle::newHandle()
{
    return std::make_shared<SimpleCancelHandleImpl>();
}

CancelHandlePtr CancelHandle::newSharedHandle()
{
    return std::make_shared<SharedCancelHandleImpl>();
}

void internal::CancellableAwaiter::await_suspend(std::coroutine_handle<> handle)
{
    static_cast<CancelHandleImpl *>(cancelHandle_.get())
        ->setCancelHandle([this, handle, loop = loop_]() {
            setException(std::make_exception_ptr(
                TaskCancelledException("Task cancelled")));
            loop->queueInLoop([handle]() { handle.resume(); });
            return;
        });
}

void internal::CancellableTimeAwaiter::await_suspend(
    std::coroutine_handle<> handle)
{
    auto execFlagPtr = std::make_shared<std::atomic_bool>(false);
    static_cast<CancelHandleImpl *>(cancelHandle_.get())
        ->setCancelHandle([this, handle, execFlagPtr, loop = loop_]() {
            if (!execFlagPtr->exchange(true))
            {
                setException(std::make_exception_ptr(
                    TaskCancelledException("Task cancelled")));
                loop->queueInLoop([handle]() { handle.resume(); });
                return;
            }
        });
    loop_->runAfter(delay_, [handle, execFlagPtr = std::move(execFlagPtr)]() {
        if (!execFlagPtr->exchange(true))
        {
            handle.resume();
        }
    });
}

}  // namespace drogon

#endif
