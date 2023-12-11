#include <drogon/HttpViewData.h>
#include <drogon/drogon_test.h>
#include <iostream>

using namespace drogon;

DROGON_TEST(HttpViewData)
{
    HttpViewData data;
    data.insert("1", 1);
    data.insertAsString("2", 2.0);
    data.insertFormattedString("3", "third value is %d", 3);
    data.insertAsString("4", "4");
    data.insert("5", 5);
    data.insert("5", std::string("5!!!!!!!"));  // Overrides the old value
    char six = 6;
    data.insert("6", six);

    CHECK(data.get<int>("1") == 1);
    CHECK(data.get<std::string>("2") == "2");
    CHECK(data.get<std::string>("3") == "third value is 3");
    CHECK(data.get<std::string>("4") == "4");
    CHECK(data.get<std::string>("5") == "5!!!!!!!");
    CHECK(data.get<char>("6") == 6);
    CHECK(data.get<int>("1") == 1);  // get a second time

    // Bad key returns a default constructed value
    CHECK_NOTHROW(data.get<int>("this_does_not_exist"));

    SUBSECTION(Translate)
    {
        CHECK(HttpViewData::needTranslation("") == false);
        CHECK(HttpViewData::needTranslation("!)(*#") == false);
        CHECK(HttpViewData::needTranslation("#include <iostream>") == true);
        CHECK(HttpViewData::needTranslation("<body></body>") == true);

        CHECK(HttpViewData::htmlTranslate("#include <iostream>") ==
              "#include &lt;iostream&gt;");
        CHECK(HttpViewData::htmlTranslate("&gt;") == "&amp;gt;");
    }
}
