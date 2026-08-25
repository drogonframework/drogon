#include <drogon/drogon_test.h>
#include <string>

using drogon::test::internal::Decomposer;

// The expansion string is what a failing assertion prints after "With
// expansion". It is only built when the assertion fails (or when successful
// tests are being printed), so every case below asserts on a false comparison.
DROGON_TEST(DrogonTestExpansion)
{
    int two = 2, one = 1;

    // Regression: operator< used to print the left operand on both sides, so
    // this expanded to "2 < 2" and gave no hint of what was compared against.
    auto lessThan = (Decomposer() <= two < one).result();
    CHECK(lessThan.first == false);
    CHECK(lessThan.second == "2 < 1");

    auto greaterThan = (Decomposer() <= one > two).result();
    CHECK(greaterThan.first == false);
    CHECK(greaterThan.second == "1 > 2");

    auto lessEqual = (Decomposer() <= two <= one).result();
    CHECK(lessEqual.first == false);
    CHECK(lessEqual.second == "2 <= 1");

    auto greaterEqual = (Decomposer() <= one >= two).result();
    CHECK(greaterEqual.first == false);
    CHECK(greaterEqual.second == "1 >= 2");

    auto equal = (Decomposer() <= one == two).result();
    CHECK(equal.first == false);
    CHECK(equal.second == "1 == 2");

    auto notEqual = (Decomposer() <= one != one).result();
    CHECK(notEqual.first == false);
    CHECK(notEqual.second == "1 != 1");

    SUBSECTION(Strings)
    {
        std::string apple{"apple"}, orange{"orange"};
        auto strings = (Decomposer() <= apple == orange).result();
        CHECK(strings.first == false);
        CHECK(strings.second == "\"apple\" == \"orange\"");
    }
}
