#include <drogon/drogon_test.h>
#include <drogon/utils/coroutine.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/net/EventLoopThread.h>
#include <atomic>
#include <coroutine>
#include <functional>
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

    // Coroutine bodies should be awaitable
    auto empty_coro = []() -> Task<> { co_return; };
    STATIC_REQUIRE(is_awaitable_v<decltype(empty_coro)> == false);

    // But their return types should be
    STATIC_REQUIRE(is_awaitable_v<decltype(empty_coro())>);

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

DROGON_TEST(AwaiterTraits)
{
    auto awaiter = sleepCoro(drogon::app().getLoop(), 0.001);
    STATIC_REQUIRE(is_awaitable_v<decltype(awaiter)>);
    STATIC_REQUIRE(std::is_void<await_result_t<decltype(awaiter)>>::value);
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
    thread_local int num{0};
    trantor::EventLoopThread thread;
    thread.run();
    thread.getLoop()->queueInLoop([]() { num = 100; });

    auto switch_thread = [TEST_CTX, &thread]() -> Task<> {
        CHECK(num == 0);
        co_await switchThreadCoro(thread.getLoop());
        CHECK(num == 100);
        thread.getLoop()->quit();
    };
    sync_wait(switch_thread());
    thread.wait();
}

DROGON_TEST(WhenAll)
{
    // Check all tasks are executed
    int counter = 0;
    auto coro = [&]() -> Task<> {
        counter++;
        co_return;
    };
    auto except = []() -> Task<> {
        throw std::runtime_error("test error");
        co_return;
    };
    auto slow = []() -> Task<> {
        co_await sleepCoro(drogon::app().getLoop(), 0.001);
        co_return;
    };
    auto return42 = []() -> Task<int> { co_return 42; };

    std::vector<Task<>> tasks;
    for (int i = 0; i < 10; ++i)
        tasks.push_back(coro());
    sync_wait(when_all(std::move(tasks)));
    CHECK(counter == 10);

    // Check exceptions are propagated while all coroutines run until completion
    counter = 0;
    std::vector<Task<>> tasks2;
    tasks2.push_back(coro());
    tasks2.push_back(except());
    tasks2.push_back(coro());

    CHECK_THROWS_AS(sync_wait(when_all(std::move(tasks2))), std::runtime_error);
    CHECK(counter == 2);

    // Check waiting for tasks that can't complete immediately works
    counter = 0;
    std::vector<Task<>> tasks3;
    tasks3.push_back(slow());
    // tasks3.push_back(slow());
    tasks3.push_back(coro());
    sync_wait(when_all(std::move(tasks3)));
    CHECK(counter == 1);

    // Check we can get the results of the tasks
    std::vector<Task<int>> tasks4;
    tasks4.push_back(return42());
    tasks4.push_back(return42());
    auto results = sync_wait(when_all(std::move(tasks4)));
    CHECK(results.size() == 2);
    CHECK(results[0] == 42);
    CHECK(results[1] == 42);
}
