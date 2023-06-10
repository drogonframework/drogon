#include <drogon/drogon_test.h>
#include <../../lib/src/SlashRemover.cc>

using std::string;

DROGON_TEST(SlashRemoverTest)
{
    const string url = "///home//page//", root = "///";

    string cleanUrl = url;
    removeTrailingSlashes(cleanUrl);
    CHECK(cleanUrl == "///home//page");

    cleanUrl = url;
    removeDuplicateSlashes(cleanUrl);
    CHECK(cleanUrl == "/home/page/");

    cleanUrl = url;
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == "/home/page");

    cleanUrl = "///home//page";
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == "/home/page");

    cleanUrl = "/home/page/";
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == "/home/page");

    cleanUrl = root;
    removeTrailingSlashes(cleanUrl);
    CHECK(cleanUrl == "/");

    cleanUrl = root;
    removeDuplicateSlashes(cleanUrl);
    CHECK(cleanUrl == "/");
}
