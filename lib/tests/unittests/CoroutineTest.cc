#include <drogon/drogon_test.h>
#include <drogon/utils/coroutine.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/net/EventLoopThread.h>
#include <type_traits>

using namespace drogon;

namespace drogon::internal
{
struct SomeStruct
{
    ~SomeStruct()
    {
        beenDestructed = true;
    }

    static bool beenDestructed;
};

bool SomeStruct::beenDestructed = false;

struct StructAwaiter : public CallbackAwaiter<std::shared_ptr<SomeStruct>>
{
    void await_suspend(std::coroutine_handle<> handle)
    {
        setValue(std::make_shared<SomeStruct>());
        handle.resume();
    }
};

}  // namespace drogon::internal

// Workaround limitation of macros
template <typename T>
using is_int = std::is_same<T, int>;
template <typename T>
using is_void = std::is_same<T, void>;

DROGON_TEST(CroutineBasics)
{
    // Basic checks making sure coroutine works as expected
    STATIC_REQUIRE(is_awaitable_v<Task<>>);
    STATIC_REQUIRE(is_awaitable_v<Task<int>>);
    STATIC_REQUIRE(is_awaitable_v<Task<>>);
    STATIC_REQUIRE(is_awaitable_v<Task<int>>);
    STATIC_REQUIRE(is_int<await_result_t<Task<int>>>::value);
    STATIC_REQUIRE(is_void<await_result_t<Task<>>>::value);

    // No, you cannot await AsyncTask. By design
    STATIC_REQUIRE(is_awaitable_v<AsyncTask> == false);

    // AsyncTask should execute eagerly
    int m = 0;
    [&m]() -> AsyncTask {
        m = 1;
        co_return;
    }();
    REQUIRE(m == 1);

    // Make sure sync_wait works
    CHECK(sync_wait([]() -> Task<int> { co_return 1; }()) == 1);

    // make sure it does affect the outside world
    int n = 0;
    sync_wait([&]() -> Task<> {
        n = 1;
        co_return;
    }());
    CHECK(n == 1);

    // Testing that exceptions can propagate through coroutines
    auto throw_in_task = [TEST_CTX]() -> Task<> {
        auto f = []() -> Task<> { throw std::runtime_error("test error"); };

        CHECK_THROWS_AS(co_await f(), std::runtime_error);
    };
    sync_wait(throw_in_task());

    // Test sync_wait propagates exception
    auto throws = []() -> Task<> {
        throw std::runtime_error("bla");
        co_return;
    };
    CHECK_THROWS_AS(sync_wait(throws()), std::runtime_error);

    // Test co_return non-copyable object works
    auto return_unique_ptr = [TEST_CTX]() -> Task<std::unique_ptr<int>> {
        co_return std::make_unique<int>(42);
    };
    CHECK(*sync_wait(return_unique_ptr()) == 42);

    // Test co_awaiting non-copyable object works
    auto await_non_copyable = [TEST_CTX]() -> Task<> {
        auto return_unique_ptr = []() -> Task<std::unique_ptr<int>> {
            co_return std::make_unique<int>(123);
        };
        auto ptr = co_await return_unique_ptr();
        CHECK(*ptr == 123);
    };
    sync_wait(await_non_copyable());

    // This only works because async_run tries to run the coroutine as soon as
    // possible and the coroutine does not wait
    int testVar = 0;
    async_run([&testVar]() -> Task<void> {
        testVar = 1;
        co_return;
    });
    CHECK(testVar == 1);
    async_run([TEST_CTX]() -> Task<void> {
        auto val =
            co_await queueInLoopCoro<int>(app().getLoop(), []() { return 42; });
        CHECK(val == 42);
    });
    async_run([TEST_CTX]() -> Task<void> {
        co_await queueInLoopCoro<void>(app().getLoop(), []() { LOG_DEBUG; });
    });
}

DROGON_TEST(CompilcatedCoroutineLifetime)
{
    auto coro = []() -> Task<Task<std::string>> {
        auto coro2 = []() -> Task<std::string> {
            auto coro3 = []() -> Task<std::string> {
                co_return std::string("Hello, World!");
            };
            auto coro4 = [coro3 = std::move(coro3)]() -> Task<std::string> {
                auto coro5 = []() -> Task<> { co_return; };
                co_await coro5();
                co_return co_await coro3();
            };
            co_return co_await coro4();
        };

        co_return coro2();
    };

    auto task1 = coro();
    auto task2 = sync_wait(task1);
    std::string str = sync_wait(task2);

    CHECK(str == "Hello, World!");
}

DROGON_TEST(CoroutineDestruction)
{
    // Test coroutine destruction
    auto destruct = []() -> Task<> {
        auto awaitStruct = []() -> Task<std::shared_ptr<internal::SomeStruct>> {
            co_return co_await internal::StructAwaiter();
        };

        auto awaitNothing = [awaitStruct]() -> Task<> {
            co_await awaitStruct();
        };

        co_await awaitNothing();
    };
    sync_wait(destruct());
    CHECK(internal::SomeStruct::beenDestructed == true);
}

DROGON_TEST(AsyncWaitLifetime)
{
    app().getLoop()->queueInLoop([TEST_CTX]() {
        async_run([TEST_CTX]() -> Task<> {
            auto ptr = std::make_shared<std::string>("test");
            CHECK(ptr.use_count() == 1);
            co_await sleepCoro(drogon::app().getLoop(), 0.01);
            CHECK(ptr.use_count() == 1);
        });
    });

    app().getLoop()->queueInLoop([TEST_CTX]() {
        auto ptr = std::make_shared<std::string>("test");
        async_run([ptr, TEST_CTX]() -> Task<> {
            CHECK(ptr.use_count() == 2);
            co_await sleepCoro(drogon::app().getLoop(), 0.01);
            CHECK(ptr.use_count() == 1);
        });
    });

    auto ptr = std::make_shared<std::string>("test");
    app().getLoop()->queueInLoop([ptr, TEST_CTX]() {
        async_run([ptr, TEST_CTX]() -> Task<> {
            co_await sleepCoro(drogon::app().getLoop(), 0.01);
            CHECK(ptr.use_count() == 1);
        });
    });

    auto ptr2 = std::make_shared<std::string>("test");
    app().getLoop()->queueInLoop(async_func([ptr2, TEST_CTX]() -> Task<> {
        co_await sleepCoro(drogon::app().getLoop(), 0.01);
        CHECK(ptr2.use_count() == 1);
    }));
}

DROGON_TEST(SwitchThread)
{
    trantor::EventLoopThread thread;
    thread.getLoop()->setIndex(12345);
    thread.run();

    auto switch_thread = [TEST_CTX, &thread]() -> Task<> {
        co_await switchThreadCoro(thread.getLoop());
        auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
        MANDATE(currentLoop != nullptr);
        CHECK(currentLoop->index() == 12345);
        currentLoop->quit();
    };
    sync_wait(switch_thread());
    thread.wait();
}

DROGON_TEST(Cancellation)
{
    using namespace drogon::internal;

    trantor::EventLoopThread thread;  // helper thread
    thread.run();

    auto testCancelTask = [TEST_CTX, loop = thread.getLoop()]() -> Task<> {
        auto cancelHandle = CancelHandle::newHandle();

        // wait coro for 10 seconds, but cancel after 1 second
        loop->runAfter(1, [cancelHandle]() { cancelHandle->cancel(); });

        int64_t start = time(nullptr);
        try
        {
            LOG_INFO << "Waiting for 10 seconds...";
            co_await sleepCoro(loop, 10, cancelHandle);
            CHECK(false);  // should not reach here
        }
        catch (const TaskCancelledException &ex)
        {
            int64_t waitTime = time(nullptr) - start;
            CHECK(waitTime < 2);
            LOG_INFO << "Oops... only waited for " << waitTime << " second(s)";
        }
    };

    sync_wait(testCancelTask());
    thread.getLoop()->quit();
    thread.wait();
}

DROGON_TEST(SharedCancellation)
{
    using namespace drogon::internal;

    trantor::EventLoopThread thread;  // helper thread
    thread.run();
    auto loop = thread.getLoop();
    auto sharedHandle = CancelHandle::newSharedHandle();

    auto testSharedCancelTask1 = [TEST_CTX, sharedHandle, loop]() -> Task<> {
        int64_t start = time(nullptr);
        try
        {
            LOG_INFO << "Waiting for 10 seconds...";
            co_await sleepCoro(loop, 10, sharedHandle);
            CHECK(false);  // should not reach here
        }
        catch (const TaskCancelledException &ex)
        {
            int64_t waitTime = time(nullptr) - start;
            CHECK(waitTime < 2);
            LOG_INFO << "Oops... only waited for " << waitTime << " second(s)";
        }
    };

    auto testSharedCancelTask2 = [TEST_CTX, sharedHandle, loop]() -> Task<> {
        int64_t start = time(nullptr);
        try
        {
            LOG_INFO << "Sleep forever...";
            co_await sleepForeverCoro(loop, sharedHandle);
            CHECK(false);  // should not reach here
        }
        catch (const TaskCancelledException &ex)
        {
            int64_t waitTime = time(nullptr) - start;
            CHECK(waitTime < 2);
            LOG_INFO << "Oops... only slept for " << waitTime << " second(s)";
        }
    };

    auto testSharedCancelTask3 = [TEST_CTX, sharedHandle, loop]() -> Task<> {
        co_await sleepCoro(loop, 1.5);
        int64_t start = time(nullptr);
        try
        {
            LOG_INFO << "Try sleep after cancel...";
            co_await sleepForeverCoro(loop, sharedHandle);
            CHECK(false);  // should not reach here
        }
        catch (const TaskCancelledException &ex)
        {
            int64_t waitTime = time(nullptr) - start;
            CHECK(waitTime < 2);
            LOG_INFO << "Oops... only slept for " << waitTime << " second(s)";
        }
    };
    // cancel both tasks after 1 second
    loop->runAfter(1, [sharedHandle]() { sharedHandle->cancel(); });
    sync_wait(testSharedCancelTask1());
    sync_wait(testSharedCancelTask2());
    sync_wait(testSharedCancelTask3());
    loop->quit();
    thread.wait();
}
