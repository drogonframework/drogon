#include <trantor/net/Resolver.h>
#include <iostream>
void dns(const std::shared_ptr<trantor::Resolver> &resolver)
{
    auto now = trantor::Date::now();
    resolver->resolve("www.baidu.com", [now](const trantor::InetAddress &addr) {
        auto interval = trantor::Date::now().microSecondsSinceEpoch() -
                        now.microSecondsSinceEpoch();
        std::cout << "baidu:" << addr.toIp() << " " << interval / 1000 << "ms"
                  << std::endl;
    });
    resolver->resolve("www.google.com",
                      [now](const trantor::InetAddress &addr) {
                          auto interval =
                              trantor::Date::now().microSecondsSinceEpoch() -
                              now.microSecondsSinceEpoch();
                          std::cout << "google:" << addr.toIp() << " "
                                    << interval / 1000 << "ms" << std::endl;
                      });
    resolver->resolve("www.sina.com", [now](const trantor::InetAddress &addr) {
        auto interval = trantor::Date::now().microSecondsSinceEpoch() -
                        now.microSecondsSinceEpoch();
        std::cout << "sina:" << addr.toIp() << " " << interval / 1000 << "ms"
                  << std::endl;
    });
    resolver->resolve("www.xjfisfjaskfeiakdjfg.com",
                      [now](const trantor::InetAddress &addr) {
                          auto interval =
                              trantor::Date::now().microSecondsSinceEpoch() -
                              now.microSecondsSinceEpoch();
                          std::cout << "bad address:" << addr.toIp() << " "
                                    << interval / 1000 << "ms" << std::endl;
                      });
    resolver->resolve("localhost", [now](const trantor::InetAddress &addr) {
        auto interval = trantor::Date::now().microSecondsSinceEpoch() -
                        now.microSecondsSinceEpoch();
        std::cout << "localhost:" << addr.toIp() << " " << interval / 1000
                  << "ms" << std::endl;
    });
}
int main()
{
    trantor::EventLoop loop;
    auto resolver = trantor::Resolver::newResolver(&loop);
    dns(resolver);
    loop.runAfter(1.0, [resolver]() { dns(resolver); });
    loop.loop();
}
