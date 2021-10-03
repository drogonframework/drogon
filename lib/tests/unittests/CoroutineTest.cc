#include <drogon/drogon_test.h>
#include <drogon/utils/coroutine.h>

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

// Workarround limitation of macros
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

    // Testing that exceptions can propergate through coroutines
    auto throw_in_task = [TEST_CTX]() -> Task<> {
        auto f = []() -> Task<> { throw std::runtime_error("test error"); };

        CHECK_THROWS_AS(co_await f(), std::runtime_error);
    };
    sync_wait(throw_in_task());

    // Test sync_wait propergrates exception
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

    // TEST launching coroutine via the async_run API
    // async_run should run the coroutine wthout waiting for anything else
    int testVar = 0;
    async_run([&testVar]() -> AsyncTask {
        testVar = 1;
        co_return;
    }());
    CHECK(testVar == 1);  // This only works because neither async_run nor the
                          // coroutine within awaits

    testVar = 0;
    async_run([&testVar]() -> AsyncTask {
        testVar = 1;
        co_return;
    });  // invoke coroutines with no parameters
    CHECK(testVar == 1);

    testVar = 0;
    async_run([&testVar]() -> Task<void> {
        testVar = 1;
        co_return;
    }());
    CHECK(testVar == 1);

    testVar = 0;
    async_run([&testVar]() -> Task<void> {
        testVar = 1;
        co_return;
    });
    CHECK(testVar == 1);
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
