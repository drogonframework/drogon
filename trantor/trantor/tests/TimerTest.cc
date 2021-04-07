#include <trantor/net/EventLoop.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <thread>
#include <chrono>
using namespace std::literals;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);
    trantor::EventLoop loop;
    auto id1 = loop.runAfter(1s, []() {
        LOG_ERROR << "This info shouldn't be displayed!";
    });
    loop.invalidateTimer(id1);
    auto id2 = loop.runEvery(0.3s, []() {
        LOG_ERROR << "This timer will be invalidated after 3 second;";
    });
    std::thread thread([id2, &loop]() {
        std::this_thread::sleep_for(3s);
        loop.invalidateTimer(id2);
    });
    thread.detach();
    loop.runEvery(3, []() { LOG_DEBUG << " runEvery 3s"; });
    loop.runAt(trantor::Date::date().after(10),
               []() { LOG_DEBUG << "runAt 10s later"; });
    loop.runAfter(5, []() { std::cout << "runAt 5s later" << std::endl; });
    loop.runEvery(1, []() { std::cout << "runEvery 1s" << std::endl; });
    loop.runAfter(4, []() { std::cout << "runAfter 4s" << std::endl; });
    loop.runAfter(10min,
                  []() { std::cout << "*********** run after 10 min\n"; });
    loop.loop();
}
