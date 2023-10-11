#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>

struct SameContent
{
    SameContent(const std::vector<std::string> &container)
        : container_(container.begin(), container.end())
    {
    }

    std::vector<std::string> container_;
};

template <typename Container1>
inline bool operator==(const Container1 &a, const SameContent &wrapper)
{
    const auto &b = wrapper.container_;
    if (a.size() != b.size())
        return false;

    auto ait = a.begin();
    auto bit = b.begin();

    while (ait != a.end() && bit != b.end())
    {
        if (*ait != *bit)
            break;
        ait++;
        bit++;
    }
    return ait == a.end() && bit == b.end();
}

using namespace drogon;

DROGON_TEST(StringOpsTest)
{
    SUBSECTION(SplitString)
    {
        std::string str = "1,2,3,3,,4";
        CHECK(utils::splitString(str, ",") ==
              SameContent({"1", "2", "3", "3", "4"}));
        CHECK(utils::splitString(str, ",", true) ==
              SameContent({"1", "2", "3", "3", "", "4"}));
        CHECK(utils::splitString(str, "|", true) ==
              SameContent({"1,2,3,3,,4"}));

        str = "a||b||c||||";
        CHECK(utils::splitString(str, "||") == SameContent({"a", "b", "c"}));
        CHECK(utils::splitString(str, "||", true) ==
              SameContent({"a", "b", "c", "", ""}));

        str = "aabbbaabbbb";
        CHECK(utils::splitString(str, "bb") == SameContent({"aa", "baa"}));
        CHECK(utils::splitString(str, "bb", true) ==
              SameContent({"aa", "baa", "", ""}));

        str = "";
        CHECK(utils::splitString(str, ",") == SameContent({}));
        CHECK(utils::splitString(str, ",", true) == SameContent({""}));
    }

    SUBSECTION(SplitStringToSet)
    {
        // splitStringToSet ignores empty strings
        std::string str = "1,2,3,3,,4";
        auto s = utils::splitStringToSet(str, ",");
        CHECK(s.size() == 4UL);
        CHECK(s.count("1") == 1UL);
        CHECK(s.count("2") == 1UL);
        CHECK(s.count("3") == 1UL);
        CHECK(s.count("4") == 1UL);

        str = "a|||a";
        s = utils::splitStringToSet(str, "||");
        CHECK(s.size() == 2UL);
        CHECK(s.count("a") == 1UL);
        CHECK(s.count("|a") == 1UL);
    }

    SUBSECTION(ReplaceAll)
    {
        std::string str = "3.14159";
        utils::replaceAll(str, "1", "a");
        CHECK(str == "3.a4a59");

        str = "aaxxxaaxxxxaaxxxxx";
        utils::replaceAll(str, "xx", "oo");
        CHECK(str == "aaooxaaooooaaoooox");
    }
}
