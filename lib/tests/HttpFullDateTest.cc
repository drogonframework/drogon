#include <drogon/utils/Utilities.h>
#include <iostream>

using namespace drogon;
int main()
{
    auto str = utils::getHttpFullDate();
    std::cout << str << std::endl;
    auto date = utils::getHttpDate(str);
    std::cout << utils::getHttpFullDate(date) << std::endl;
}