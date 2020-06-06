#include <drogon/drogon.h>
#include <thread>
#include <chrono>

/**
 * @brief This test program tests to call the app().run() in another thread.
 *
 */
int main()
{
    std::thread([]() {
        drogon::app().getLoop()->runEvery(1, []() {
            std::cout << "!" << std::endl;
        });
    }).detach();

    std::thread l([]() { drogon::app().run(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    trantor::EventLoop loop;
    l.join();
}