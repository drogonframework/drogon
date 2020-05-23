#include <gtest/gtest.h>
#include <drogon/PubSubService.h>
#include <string>
#include <iostream>

TEST(PubSubServiceTest, normal)
{
    drogon::PubSubService<std::string> service;
    service.subscribe("topic1",
                      [](const std::string &topic, const std::string &message) {
                          EXPECT_STREQ(topic.c_str(), "topic1");
                          EXPECT_STREQ(message.c_str(), "hello world");
                      });
    service.publish("topic1", "hello world");
    service.publish("topic2", "hello world");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}