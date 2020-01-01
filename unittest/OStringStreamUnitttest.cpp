#include <gtest/gtest.h>
#include <drogon/utils/OStringStream.h>
#include <string>
#include <iostream>

TEST(OStringStreamTest, interger)
{
    drogon::OStringStream ss;
    ss << 12;
    ss << 345L;
    EXPECT_EQ(ss.str(), "12345");
}

TEST(OStringStreamTest, float_number)
{
    drogon::OStringStream ss;
    ss << 3.14f;
    ss << 3.1416;
    EXPECT_EQ(ss.str(), "3.143.1416");
}

TEST(OStringStreamTest, literal_string)
{
    drogon::OStringStream ss;
    ss << "hello";
    ss << " world";
    EXPECT_EQ(ss.str(), "hello world");
}

TEST(OStringStreamTest, string_view)
{
    drogon::OStringStream ss;
    ss << drogon::string_view("hello");
    ss << drogon::string_view(" world");
    EXPECT_EQ(ss.str(), "hello world");
}

TEST(OStringStreamTest, std_string)
{
    drogon::OStringStream ss;
    ss << std::string("hello");
    ss << std::string(" world");
    EXPECT_EQ(ss.str(), "hello world");
}

TEST(OStringStreamTest, mix)
{
    drogon::OStringStream ss;
    ss << std::string("hello");
    ss << drogon::string_view(" world");
    ss << "!";
    ss << 123;
    ss << 3.14f;

    EXPECT_EQ(ss.str(), "hello world!1233.14");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
