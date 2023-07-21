#include <drogon/drogon_test.h>
#include <../../lib/src/SlashRemover.cc>

using std::string;

DROGON_TEST(SlashRemoverTest)
{
    const string url = "///home//page//", urlNoTrail = "///home//page",
                 urlNoDup = "/home/page/", urlNoExcess = "/home/page",
                 root = "///", rootNoExcess = "/";

    string cleanUrl = url;
    removeTrailingSlashes(cleanUrl);
    CHECK(cleanUrl == urlNoTrail);

    cleanUrl = url;
    removeDuplicateSlashes(cleanUrl);
    CHECK(cleanUrl == urlNoDup);

    cleanUrl = url;
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == urlNoExcess);

    cleanUrl = urlNoTrail;
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == urlNoExcess);

    cleanUrl = urlNoDup;
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == urlNoExcess);

    cleanUrl = root;
    removeTrailingSlashes(cleanUrl);
    CHECK(cleanUrl == rootNoExcess);

    cleanUrl = root;
    removeDuplicateSlashes(cleanUrl);
    CHECK(cleanUrl == rootNoExcess);

    cleanUrl = root;
    removeExcessiveSlashes(cleanUrl);
    CHECK(cleanUrl == rootNoExcess);
}
