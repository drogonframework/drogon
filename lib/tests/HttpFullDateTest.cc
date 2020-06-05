#include <drogon/utils/Utilities.h>
#include <iostream>

using namespace drogon;
int main()
{
    auto str = utils::getHttpFullDate();
    std::cout << str << std::endl;
    auto date = utils::getHttpDate(str);
    std::cout << utils::getHttpFullDate(date) << std::endl;

    {
        // rfc 850
        auto date = utils::getHttpDate("Fri, 05-Jun-20 09:19:38 GMT");
        std::cout << (date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC)
                  << " should be " << 1591348778 << std::endl;
    }
    {
        // reddit format
        auto date = utils::getHttpDate("Fri, 05-Jun-2020 09:19:38 GMT");
        std::cout << (date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC)
                  << " should be " << 1591348778 << std::endl;
    }
    {
        // asctime format
        auto epoch = time(nullptr);
        auto str = asctime(localtime(&epoch));
        auto date = utils::getHttpDate(str);
        std::cout << (date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC)
                  << " should be " << epoch << std::endl;
    }
    {
        // invalid format
        auto date = utils::getHttpDate("Fri, this format is invalid");
        std::cout << date.microSecondsSinceEpoch() << " should be " << -1
                  << std::endl;
    }
}