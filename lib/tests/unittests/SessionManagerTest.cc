#include "../../lib/src/SessionManager.h"
#include <drogon/drogon_test.h>
#include <trantor/net/EventLoopThread.h>

using namespace drogon;

DROGON_TEST(SessionManagerTest)
{
    trantor::EventLoopThread loopThread;
    loopThread.run();
    std::vector<AdviceStartSessionCallback> startAdvices;
    std::vector<AdviceDestroySessionCallback> destroyAdvices;
    int generated = 0;
    SessionManager manager(
        loopThread.getLoop(), 0, startAdvices, destroyAdvices, [&generated]() {
            return "server-id-" + std::to_string(++generated);
        });

    // A new session under a server-generated ID is stored.
    auto session = manager.getSession("server-id-0", true);
    CHECK(session->sessionId() == "server-id-0");

    // The client sends that ID back: the same session is returned.
    CHECK(manager.getSession("server-id-0", false) == session);
    CHECK(generated == 0);

    // An ID the server never issued is not adopted: the client gets a new
    // session with a fresh server-generated ID.
    auto other = manager.getSession("attacker-chosen", false);
    CHECK(other != session);
    CHECK(other->sessionId() == "server-id-1");
    CHECK(generated == 1);

    // The new session is reachable under its own ID, and the unknown ID still
    // isn't.
    CHECK(manager.getSession("server-id-1", false) == other);
    auto again = manager.getSession("attacker-chosen", false);
    CHECK(again->sessionId() == "server-id-2");
    CHECK(again != other);
}
