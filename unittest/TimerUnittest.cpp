#include <trantor/net/EventLoop.h>
#include <gtest/gtest.h>
#include <string>
#include <iostream>
using namespace trantor;
using namespace std::literals;
EventLoop loop;

TEST(TimerTest, RunEvery)
{
    size_t count{0};
    std::chrono::steady_clock::time_point tp{std::chrono::steady_clock::now()};
    loop.runEvery(0.1s, [&count, tp] {
        ++count;
        if (count == 10)
        {
            auto d = std::chrono::steady_clock::now() - tp;
            auto err = d > 1s ? d - 1s : 1s - d;
            auto msErr =
                std::chrono::duration_cast<std::chrono::milliseconds>(err)
                    .count();
            EXPECT_LT(msErr, 100LL);
            loop.quit();
            return;
        }
        std::this_thread::sleep_for(0.02s);
    });
    loop.runAfter(0.35s, [tp] {
        auto d = std::chrono::steady_clock::now() - tp;
        auto err = d > 0.35s ? d - 0.35s : 0.35s - d;
        auto msErr =
            std::chrono::duration_cast<std::chrono::milliseconds>(err).count();
        EXPECT_LT(msErr, 30LL);
    });
    loop.loop();
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}