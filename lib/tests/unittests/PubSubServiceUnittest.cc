#include <drogon/PubSubService.h>
#include <drogon/drogon_test.h>

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
