#include <drogon/drogon_test.h>
#include <../../lib/src/SlashRemover.cc>
#include <string>

using std::string;

DROGON_TEST(SlashRemoverTest)
{
    const string url = "///home//page//", urlNoTrail = "///home//page",
                 urlNoDup = "/home/page/", urlNoExcess = "/home/page",
                 root = "///", rootNoExcess = "/";

    string cleanUrl;

    // Full
    removeTrailingSlashes(cleanUrl, findTrailingSlashes(url), url);
    CHECK(cleanUrl == urlNoTrail);

    cleanUrl = url;
    removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url));
    CHECK(cleanUrl == urlNoDup);

    removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
    CHECK(cleanUrl == urlNoExcess);

    // Partial
    removeExcessiveSlashes(cleanUrl,
                           findExcessiveSlashes(urlNoTrail),
                           urlNoTrail);
    CHECK(cleanUrl == urlNoExcess);

    removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(urlNoDup), urlNoDup);
    CHECK(cleanUrl == urlNoExcess);

    // Root
    cleanUrl = root;
    removeTrailingSlashes(cleanUrl, findTrailingSlashes(root), root);
    CHECK(cleanUrl == rootNoExcess);

    cleanUrl = root;
    removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(root));
    CHECK(cleanUrl == rootNoExcess);

    removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(root), root);
    CHECK(cleanUrl == rootNoExcess);

    // Test precedence when both match the same place
    const string url2 = "/a///", url2NoDup = "/a/", url2NoExcess = "/a";

    removeTrailingSlashes(cleanUrl, findTrailingSlashes(url2), url2);
    CHECK(cleanUrl == url2NoExcess);

    cleanUrl = url2;
    removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url2));
    CHECK(cleanUrl == url2NoDup);

    removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url2), url2);
    CHECK(cleanUrl == url2NoExcess);

    // Test when trail does not match
    const string url3 = "///a", url3NoExcess = "/a";

    cleanUrl.clear();
    {
        auto find = findTrailingSlashes(url3);
        if (find != string::npos)
            removeTrailingSlashes(cleanUrl, find, url3);
    }
    CHECK(cleanUrl.empty());

    cleanUrl = url3;
    removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url3));
    CHECK(cleanUrl == url3NoExcess);

    removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url3), url3);
    CHECK(cleanUrl == url3NoExcess);

    // Test when duplicate does not match
    const string url4 = "/a/", url4NoExcess = "/a";

    removeTrailingSlashes(cleanUrl, findTrailingSlashes(url4), url4);
    CHECK(cleanUrl == url4NoExcess);

    cleanUrl = url4;
    {
        auto find = findDuplicateSlashes(cleanUrl);
        if (find != string::npos)
            removeDuplicateSlashes(cleanUrl, find);
    }
    CHECK(cleanUrl == url4);

    cleanUrl = url4;
    removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url4), url4);
    CHECK(cleanUrl == url4NoExcess);

    // Test when neither match
    const string url5 = "/a";

    cleanUrl.clear();
    {
        auto find = findTrailingSlashes(url5);
        if (find != string::npos)
            removeTrailingSlashes(cleanUrl, find, url5);
    }
    CHECK(cleanUrl.empty());

    cleanUrl = url5;
    {
        auto find = findDuplicateSlashes(cleanUrl);
        if (find != string::npos)
            removeDuplicateSlashes(cleanUrl, find);
    }
    CHECK(cleanUrl == url5);

    cleanUrl.clear();
    {
        auto find = findExcessiveSlashes(url5);
        if (find.first != string::npos || find.second != string::npos)
            removeExcessiveSlashes(cleanUrl, find, url5);
    }
    CHECK(cleanUrl.empty());
}
