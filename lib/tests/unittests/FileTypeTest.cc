#include "../lib/src/HttpUtils.h"
#include <drogon/drogon_test.h>
#include <string>
using namespace drogon;

DROGON_TEST(ExtensionTest)
{
    SUBSECTION(normal)
    {
        std::string str{"drogon.jpg"};
        CHECK(getFileExtension(str) == "jpg");
    }

    SUBSECTION(negative)
    {
        std::string str{"drogon."};
        CHECK(getFileExtension(str) == "");
        str = "drogon";
        CHECK(getFileExtension(str) == "");
        str = "";
        CHECK(getFileExtension(str) == "");
        str = "....";
        CHECK(getFileExtension(str) == "");
    }
}

DROGON_TEST(FileTypeTest)
{
    SUBSECTION(normal)
    {
        CHECK(parseFileType("jpg") == FT_IMAGE);
        CHECK(parseFileType("mp4") == FT_MEDIA);
        CHECK(parseFileType("csp") == FT_CUSTOM);
        CHECK(parseFileType("html") == FT_DOCUMENT);
    }

    SUBSECTION(negative)
    {
        CHECK(parseFileType("") == FT_UNKNOWN);
        CHECK(parseFileType("don'tknow") == FT_CUSTOM);
    }
}