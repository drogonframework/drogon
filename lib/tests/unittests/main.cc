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
    CHECK_THROWS_AS(throw std::domain_error("test exception"),
                    std::domain_error);
    CHECK_NOTHROW([] { return 0; }());
    STATIC_REQUIRE(std::is_standard_layout<int>::value);
    STATIC_REQUIRE(std::is_default_constructible<test::Case>::value == false);

    auto child_test = std::make_shared<test::Case>(TEST_CTX, "ChildTest");
    CHECK(child_test->fullname() == "TestFrameworkSelfTest.ChildTest");

    // Unlike Catch2, a subsection in drogon test does not provide a fixture
    // It's only a way to signify testing different for things
    SUBSECTION(Subsection)
    {
        CHECK(TEST_CTX->fullname() == "TestFrameworkSelfTest.Subsection");
    }
}

int main(int argc, char** argv)
{
    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    std::thread thr([&]() {
        p1.set_value();
        app().run();
    });

    f1.get();
    int testStatus = test::run(argc, argv);
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return testStatus;
}
