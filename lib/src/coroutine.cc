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
    CancelHandleImpl() = default;

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

    void setCancelHandle(std::function<void()> handle)
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

CancelHandlePtr CancelHandle::create()
{
    return std::make_shared<CancelHandleImpl>();
}

void internal::CancellableAwaiter::await_suspend(std::coroutine_handle<> handle)
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
