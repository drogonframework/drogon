#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_test.h>
#include <string>
#include <iostream>

using namespace drogon;

struct TestCookie
{
    TestCookie(std::shared_ptr<test::CaseBase> ctx) : TEST_CTX(ctx)
    {
    }
    ~TestCookie()
    {
        if (!taken)
            FAIL("Test cookie not taken");
        else
            SUCCESS();
    }
    void take()
    {
        taken = true;
    }

  protected:
    bool taken = false;
    std::shared_ptr<test::CaseBase> TEST_CTX;
};

DROGON_TEST(MainLoopTest)
{
    auto cookie = std::make_shared<TestCookie>(TEST_CTX);
    drogon::app().getLoop()->queueInLoop([cookie]() { cookie->take(); });

    std::thread t([TEST_CTX]() {
        auto cookie2 = std::make_shared<TestCookie>(TEST_CTX);
        drogon::app().getLoop()->queueInLoop([cookie2]() { cookie2->take(); });
    });
    t.join();
}
