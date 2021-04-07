#include <trantor/net/EventLoop.h>
#include <trantor/utils/Logger.h>
#include <iostream>
int main()
{
    trantor::EventLoop loop;
    LOG_FATAL << trantor::Date::date().roundDay().microSecondsSinceEpoch();
    trantor::Date begin = trantor::Date::date().roundSecond().after(2);
    auto id = loop.runAt(begin, [begin, &loop]() {
        LOG_DEBUG << "test begin:";
        srand((unsigned int)time(NULL));
        for (int i = 0; i < 10000; ++i)
        {
            int aa = rand() % 10000;
            double s = (double)aa / 1000.0 + 1;
            loop.runAt(begin.after(s),
                       [s]() { LOG_ERROR << "run After:" << s; });
        }
        LOG_DEBUG << "timer created!";
    });
    std::cout << "id=" << id << std::endl;
    loop.loop();
}
