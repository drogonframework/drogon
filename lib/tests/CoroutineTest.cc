#include <drogon/utils/coroutine.h>
#include <exception>
#include <memory>
#include <type_traits>
#include <iostream>

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

int main()
{
    // Basic checks making sure coroutine works as expected
    static_assert(is_awaitable_v<Task<>>);
    static_assert(is_awaitable_v<Task<int>>);
    static_assert(std::is_same_v<await_result_t<Task<int>>, int>);
    static_assert(std::is_same_v<await_result_t<Task<>>, void>);
    static_assert(is_awaitable_v<Task<>>);
    static_assert(is_awaitable_v<Task<int>>);

    // No, you cannot await AsyncTask. By design
    static_assert(is_awaitable_v<AsyncTask> == false);

    int n = 0;
    sync_wait([&]() -> Task<> {
        n = 1;
        co_return;
    }());
    if (n != 1)
    {
        std::cerr
            << "Expected coroutine to change external valud. Didn't happen\n";
        exit(1);
    }

    // Make sure sync_wait works
    if (sync_wait([]() -> Task<int> { co_return 1; }()) != 1)
    {
        std::cerr << "Expected coroutine return 1. Didn't get that\n";
        exit(1);
    }

    // co_future converts coroutine into futures. Doesn't work for now
    //  as fut.get() waits for the coroutine. Yet the coroutine needs
    //  the same thread to resume execution for it to return a value.
    //  Thus causing a dead lock
    // auto fut = co_future([]() -> Task<std::string> { co_return "zxc"; }());
    // if (fut.get() != "zxc")
    // {
    //     std::cerr << "Expected future return \'zxc\'. Didn't get that\n";
    //     exit(1);
    // }

    // Testing that exceptions can propergate through coroutines
    auto throw_in_task = []() -> Task<> {
        auto f = []() -> Task<> { throw std::runtime_error("test error"); };

        try
        {
            co_await f();
            std::cerr << "Exception should have been thrown\n";
            exit(1);
        }
        catch (const std::exception& e)
        {
            if (std::string(e.what()) != "test error")
            {
                std::cerr << "Not the right exception\n";
                exit(1);
            }
        }
        catch (...)
        {
            std::cerr << "Shouldn't reach here\n";
            exit(1);
        }
        co_return;
    };
    sync_wait(throw_in_task());

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
    if (internal::SomeStruct::beenDestructed == false)
    {
        std::cerr << "Coroutine didn't destruct allocated object.\n";
        exit(1);
    }

    std::cout << "Done testing coroutines. No error." << std::endl;
}
