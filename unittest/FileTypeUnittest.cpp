#include "../lib/src/HttpUtils.h"
#include <gtest/gtest.h>
#include <string>
using namespace drogon;

TEST(ExtensionTest, normal)
{
    std::string str{"drogon.jpg"};
    EXPECT_TRUE(getFileExtension(str) == "jpg");
}

TEST(ExtensionTest, negative)
{
    std::string str{"drogon."};
    EXPECT_TRUE(getFileExtension(str) == "");
    str = "drogon";
    EXPECT_TRUE(getFileExtension(str) == "");
    str = "";
    EXPECT_TRUE(getFileExtension(str) == "");
    str = "....";
    EXPECT_TRUE(getFileExtension(str) == "");
}

TEST(FileTypeTest, normal)
{
    EXPECT_EQ(parseFileType("jpg"), FT_IMAGE);
    EXPECT_EQ(parseFileType("mp4"), FT_MEDIA);
    EXPECT_EQ(parseFileType("csp"), FT_CUSTOM);
    EXPECT_EQ(parseFileType("html"), FT_DOCUMENT);
}

TEST(FileTypeTest, negative)
{
    EXPECT_EQ(parseFileType(""), FT_UNKNOWN);
    EXPECT_EQ(parseFileType("don'tknow"), FT_CUSTOM);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
