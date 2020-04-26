#include <drogon/utils/Utilities.h>
#include <gtest/gtest.h>
#include <string>

TEST(Base64, base64)
{
    std::string in{"drogon framework"};
    auto out = drogon::utils::base64Encode((const unsigned char *)in.data(),
                                           in.length());
    auto out2 = drogon::utils::base64Decode(out);
    EXPECT_EQ(out, "ZHJvZ29uIGZyYW1ld29yaw==");
    EXPECT_EQ(out2, in);
}

TEST(Base64, base64_long_string)
{
    std::string in;
    for (int i = 0; i < 100000; ++i)
    {
        in.append(1, char(i));
    }
    auto out = drogon::utils::base64Encode((const unsigned char *)in.data(),
                                           in.length());
    auto out2 = drogon::utils::base64Decode(out);
    EXPECT_EQ(out2, in);
}

TEST(Base64, base64_url)
{
    std::string in{"drogon framework"};
    auto out = drogon::utils::base64Encode((const unsigned char *)in.data(),
                                           in.length(),
                                           true);
    auto out2 = drogon::utils::base64Decode(out);
    EXPECT_EQ(out, "ZHJvZ29uIGZyYW1ld29yaw==");
    EXPECT_EQ(out2, in);
}

TEST(Base64, base64_long_string_url)
{
    std::string in;
    for (int i = 0; i < 100000; ++i)
    {
        in.append(1, char(i));
    }
    auto out = drogon::utils::base64Encode((const unsigned char *)in.data(),
                                           in.length(),
                                           true);
    auto out2 = drogon::utils::base64Decode(out);
    EXPECT_EQ(out2, in);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
