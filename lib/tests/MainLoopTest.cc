#include <drogon/drogon.h>
#include <thread>
#include <unistd.h>

int main()
{
    std::thread([]() { drogon::app().getLoop()->runEvery(1, []() { std::cout << "!" << std::endl; }); }).detach();
    drogon::app().run();
}