#include <trantor/utils/Date.h>
#include <gtest/gtest.h>
#include <trantor/utils/Funcs.h>
#include <iostream>
using namespace trantor;
TEST(splitString, ACCEPT_EMPTY_STRING1)
{
    std::string originString = "1,2,3";
    auto out = splitString(originString, ",", true);
    EXPECT_EQ(out.size(), 3);
}
TEST(splitString, ACCEPT_EMPTY_STRING2)
{
    std::string originString = ",1,2,3";
    auto out = splitString(originString, ",", true);
    EXPECT_EQ(out.size(), 4);
    EXPECT_TRUE(out[0].empty());
}
TEST(splitString, ACCEPT_EMPTY_STRING3)
{
    std::string originString = ",1,2,3,";
    auto out = splitString(originString, ",", true);
    EXPECT_EQ(out.size(), 5);
    EXPECT_TRUE(out[0].empty());
}
TEST(splitString, ACCEPT_EMPTY_STRING4)
{
    std::string originString = ",1,2,3,";
    auto out = splitString(originString, ":", true);
    EXPECT_EQ(out.size(), 1);
    EXPECT_STREQ(out[0].data(), ",1,2,3,");
}
TEST(splitString, ACCEPT_EMPTY_STRING5)
{
    std::string originString = "trantor::splitString";
    auto out = splitString(originString, "::", true);
    EXPECT_EQ(out.size(), 2);
    EXPECT_STREQ(out[0].data(), "trantor");
    EXPECT_STREQ(out[1].data(), "splitString");
}
TEST(splitString, ACCEPT_EMPTY_STRING6)
{
    std::string originString = "trantor::::splitString";
    auto out = splitString(originString, "::", true);
    EXPECT_EQ(out.size(), 3);
    EXPECT_STREQ(out[0].data(), "trantor");
    EXPECT_STREQ(out[1].data(), "");
    EXPECT_STREQ(out[2].data(), "splitString");
}
TEST(splitString, ACCEPT_EMPTY_STRING7)
{
    std::string originString = "trantor:::splitString";
    auto out = splitString(originString, "::", true);
    EXPECT_EQ(out.size(), 2);
    EXPECT_STREQ(out[0].data(), "trantor");
    EXPECT_STREQ(out[1].data(), ":splitString");
}
TEST(splitString, ACCEPT_EMPTY_STRING8)
{
    std::string originString = "trantor:::splitString";
    auto out = splitString(originString, "trantor:::splitString", true);
    EXPECT_EQ(out.size(), 2);
    EXPECT_STREQ(out[0].data(), "");
    EXPECT_STREQ(out[1].data(), "");
}
TEST(splitString, ACCEPT_EMPTY_STRING9)
{
    std::string originString = "";
    auto out = splitString(originString, ",", true);
    EXPECT_EQ(out.size(), 1);
    EXPECT_STREQ(out[0].data(), "");
}
TEST(splitString, ACCEPT_EMPTY_STRING10)
{
    std::string originString = "trantor";
    auto out = splitString(originString, "", true);
    EXPECT_EQ(out.size(), 0);
}
TEST(splitString, ACCEPT_EMPTY_STRING11)
{
    std::string originString = "";
    auto out = splitString(originString, "", true);
    EXPECT_EQ(out.size(), 0);
}

TEST(splitString, NO_ACCEPT_EMPTY_STRING1)
{
    std::string originString = ",1,2,3";
    auto out = splitString(originString, ",");
    EXPECT_EQ(out.size(), 3);
    EXPECT_STREQ(out[0].data(), "1");
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING2)
{
    std::string originString = ",1,2,3,";
    auto out = splitString(originString, ",");
    EXPECT_EQ(out.size(), 3);
    EXPECT_STREQ(out[0].data(), "1");
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING3)
{
    std::string originString = ",1,2,3,";
    auto out = splitString(originString, ":");
    EXPECT_EQ(out.size(), 1);
    EXPECT_STREQ(out[0].data(), ",1,2,3,");
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING4)
{
    std::string originString = "trantor::splitString";
    auto out = splitString(originString, "::");
    EXPECT_EQ(out.size(), 2);
    EXPECT_STREQ(out[0].data(), "trantor");
    EXPECT_STREQ(out[1].data(), "splitString");
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING5)
{
    std::string originString = "trantor::::splitString";
    auto out = splitString(originString, "::");
    EXPECT_EQ(out.size(), 2);
    EXPECT_STREQ(out[0].data(), "trantor");
    EXPECT_STREQ(out[1].data(), "splitString");
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING6)
{
    std::string originString = "trantor:::splitString";
    auto out = splitString(originString, "::");
    EXPECT_EQ(out.size(), 2);
    EXPECT_STREQ(out[0].data(), "trantor");
    EXPECT_STREQ(out[1].data(), ":splitString");
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING7)
{
    std::string originString = "trantor:::splitString";
    auto out = splitString(originString, "trantor:::splitString");
    EXPECT_EQ(out.size(), 0);
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING8)
{
    std::string originString = "";
    auto out = splitString(originString, ",");
    EXPECT_EQ(out.size(), 0);
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING9)
{
    std::string originString = "trantor";
    auto out = splitString(originString, "");
    EXPECT_EQ(out.size(), 0);
}
TEST(splitString, NO_ACCEPT_EMPTY_STRING10)
{
    std::string originString = "";
    auto out = splitString(originString, "");
    EXPECT_EQ(out.size(), 0);
}
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}