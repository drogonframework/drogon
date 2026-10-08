#include <drogon/PubSubService.h>
#include <drogon/drogon_test.h>

#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

DROGON_TEST(PubSubServiceTest)
{
    drogon::PubSubService<std::string> service;
    auto id = service.subscribe("topic1",
                                [TEST_CTX](const std::string &topic,
                                           const std::string &message) {
                                    CHECK(topic == "topic1");
                                    CHECK(message == "hello world");
                                });
    service.publish("topic1", "hello world");
    service.publish("topic2", "hello world");
    CHECK(service.size() == 1UL);
    service.unsubscribe("topic1", id);
    CHECK(service.size() == 0UL);
}

DROGON_TEST(PubSubCallbackAllowsSubscriptionChanges)
{
    drogon::PubSubService<std::string> service;
    std::promise<void> callbackStarted;
    std::promise<void> subscriptionFinished;
    auto callbackStartedFuture = callbackStarted.get_future();
    auto subscriptionFinishedFuture = subscriptionFinished.get_future();

    std::thread subscriber([&]() {
        callbackStartedFuture.wait();
        service.subscribe("topic",
                          [](const std::string &, const std::string &) {});
        subscriptionFinished.set_value();
    });

    service.subscribe("topic", [&](const std::string &, const std::string &) {
        callbackStarted.set_value();
        CHECK(subscriptionFinishedFuture.wait_for(5s) ==
              std::future_status::ready);
    });
    service.publish("topic", "message");
    subscriber.join();
}
