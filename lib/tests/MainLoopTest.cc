#include <drogon/drogon.h>
#include <thread>

int main()
{
    std::thread([]() {
        drogon::app().getLoop()->runEvery(1, []() {
            std::cout << "!" << std::endl;
        });
    }).detach();
    drogon::app().run();
}