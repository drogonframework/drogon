#include <drogon/drogon_test.h>
#include <../../lib/src/SlashRemover.cc>
#include <string>

using std::string;

DROGON_TEST(SlashRemoverTest)
{
    string cleanUrl;

    {  // Regular URL
        const string urlNoTrail = "///home//page", urlNoDup = "/home/page/",
                     urlNoExcess = "/home/page";

        SUBSECTION(Full)
        {
            const string url = "///home//page//";
            removeTrailingSlashes(cleanUrl, findTrailingSlashes(url), url);
            CHECK(cleanUrl == urlNoTrail);

            cleanUrl = url;
            removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url));
            CHECK(cleanUrl == urlNoDup);

            removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);
        }

        SUBSECTION(Partial)
        {
            removeExcessiveSlashes(cleanUrl,
                                   findExcessiveSlashes(urlNoTrail),
                                   urlNoTrail);
            CHECK(cleanUrl == urlNoExcess);

            removeExcessiveSlashes(cleanUrl,
                                   findExcessiveSlashes(urlNoDup),
                                   urlNoDup);
            CHECK(cleanUrl == urlNoExcess);
        }
    }

    SUBSECTION(Root)
    {
        const string urlNoExcess = "/";

        {  // Overlapping indices
            const string url = "//";

            cleanUrl = url;
            removeTrailingSlashes(cleanUrl, findTrailingSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);

            cleanUrl = url;
            removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url));
            CHECK(cleanUrl == urlNoExcess);

            removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);
        }

        {  // Intersecting indices
            const string url = "///";

            cleanUrl = url;
            removeTrailingSlashes(cleanUrl, findTrailingSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);

            cleanUrl = url;
            removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url));
            CHECK(cleanUrl == urlNoExcess);

            removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);
        }
    }

    SUBSECTION(Overlap)
    {
        const string urlNoDup = "/a/", urlNoExcess = "/a";

        {  // Overlapping indices
            const string url = "/a//";

            removeTrailingSlashes(cleanUrl, findTrailingSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);

            cleanUrl = url;
            removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url));
            CHECK(cleanUrl == urlNoDup);

            removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);
        }

        {  // Intersecting indices
            const string url = "/a///";

            removeTrailingSlashes(cleanUrl, findTrailingSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);

            cleanUrl = url;
            removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url));
            CHECK(cleanUrl == urlNoDup);

            removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
            CHECK(cleanUrl == urlNoExcess);
        }
    }

    SUBSECTION(NoTrail)
    {
        const string url = "//a", urlNoExcess = "/a";

        cleanUrl.clear();
        {
            auto find = findTrailingSlashes(url);
            if (find != string::npos)
                removeTrailingSlashes(cleanUrl, find, url);
        }
        CHECK(cleanUrl.empty());

        cleanUrl = url;
        removeDuplicateSlashes(cleanUrl, findDuplicateSlashes(url));
        CHECK(cleanUrl == urlNoExcess);

        removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
        CHECK(cleanUrl == urlNoExcess);
    }

    SUBSECTION(NoDuplicate)
    {
        const string url = "/a/", urlNoExcess = "/a";

        removeTrailingSlashes(cleanUrl, findTrailingSlashes(url), url);
        CHECK(cleanUrl == urlNoExcess);

        cleanUrl = url;
        {
            auto find = findDuplicateSlashes(cleanUrl);
            if (find != string::npos)
                removeDuplicateSlashes(cleanUrl, find);
        }
        CHECK(cleanUrl == url);

        cleanUrl = url;
        removeExcessiveSlashes(cleanUrl, findExcessiveSlashes(url), url);
        CHECK(cleanUrl == urlNoExcess);
    }

    SUBSECTION(None)
    {
        const string url = "/a";

        cleanUrl.clear();
        {
            auto find = findTrailingSlashes(url);
            if (find != string::npos)
                removeTrailingSlashes(cleanUrl, find, url);
        }
        CHECK(cleanUrl.empty());

        cleanUrl = url;
        {
            auto find = findDuplicateSlashes(cleanUrl);
            if (find != string::npos)
                removeDuplicateSlashes(cleanUrl, find);
        }
        CHECK(cleanUrl == url);

        cleanUrl.clear();
        {
            auto find = findExcessiveSlashes(url);
            if (find.first != string::npos || find.second != string::npos)
                removeExcessiveSlashes(cleanUrl, find, url);
        }
        CHECK(cleanUrl.empty());
    }
}
