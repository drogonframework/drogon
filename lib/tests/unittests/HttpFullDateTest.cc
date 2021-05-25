#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>
#include <iostream>

using namespace drogon;
DROGON_TEST(HttpFullDateTest)
{
    auto str = utils::getHttpFullDate();
    auto date = utils::getHttpDate(str);
    CHECK(utils::getHttpFullDate(date) == str);
}
