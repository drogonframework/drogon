#include <drogon/drogon_test.h>
#include <../../lib/src/SlashRemover.cc>

using std::string;

DROGON_TEST(SlashRemoverTest)
{
    const string url = "///home//", root = "///";

    string cleanUrl = url;
    removeTrailingSlashes(cleanUrl);
    CHECK(cleanUrl == "///home");

    cleanUrl = url;
    removeDuplicateSlashes(cleanUrl);
    CHECK(cleanUrl == "/home/");

    cleanUrl = url;
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == "/home");

    cleanUrl = root;
    removeTrailingSlashes(cleanUrl);
    CHECK(cleanUrl == "/");

    cleanUrl = root;
    removeDuplicateSlashes(cleanUrl);
    CHECK(cleanUrl == "/");
}
