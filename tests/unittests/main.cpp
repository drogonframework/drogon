#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/HttpAppFramework.h>

using namespace drogon;
using namespace trantor;

DROGON_TEST(TestFrameworkSelfTest)
{
    CHECK(TEST_CTX->name() == "TestFrameworkSelfTest");
    CHECK(true);
    CHECK(false != true);
    CHECK(1 * 2 == 1 + 1);
    CHECK(42 < 100);
    CHECK(0xff <= 255);
    CHECK('a' >= 'a');
    CHECK(3.14159 > 2.71828);
    CHECK(nullptr == nullptr);
    CHECK_THROWS(throw std::runtime_error("test exception"));
    CHECK_THROWS_AS(throw std::domain_error("test exception"), std::domain_error);
    CHECK_NOTHROW([] { return 0; }());
    STATIC_REQUIRE(std::is_standard_layout<int>::value);
    STATIC_REQUIRE(std::is_default_constructible<test::Case>::value == false);

    auto child_test = std::make_shared<test::Case>(TEST_CTX, "ChildTest");
    CHECK(child_test->fullname() == "TestFrameworkSelfTest.ChildTest");
}

int main(int argc, char** argv)
{
    return drogon::test::run(argc, argv);
}
