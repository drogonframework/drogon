#include <drogon/utils/Utilities.h>
#include <gtest/gtest.h>
using namespace drogon;

TEST(HttpDate, rfc850)
{
    auto date = utils::getHttpDate("Fri, 05-Jun-20 09:19:38 GMT");
    EXPECT_EQ(date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC, 1591348778);
}

TEST(HttpDate, redditFormat)
{
    auto date = utils::getHttpDate("Fri, 05-Jun-2020 09:19:38 GMT");
    EXPECT_EQ(date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC, 1591348778);
}

TEST(HttpDate, invalidFormat)
{
    auto date = utils::getHttpDate("Fri, this format is invalid");
    EXPECT_EQ(date.microSecondsSinceEpoch(), (std::numeric_limits<int64_t>::max)());
}

TEST(HttpDate, asctimeFormat)
{
    auto epoch = time(nullptr);
    auto str = asctime(gmtime(&epoch));
    auto date = utils::getHttpDate(str);
    EXPECT_EQ(date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC, epoch);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}