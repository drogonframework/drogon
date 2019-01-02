#include <drogon/utils/Utilities.h>
#include <iostream>

using namespace drogon;
int main()
{
    auto str = getHttpFullDate();
    std::cout << str << std::endl;
    auto date = getHttpDate(str);
    std::cout << getHttpFullDate(date) << std::endl;
}