#include <string_view>
#include <drogon/utils/Utilities.h>
#include <iostream>
#include <drogon/drogon_test.h>

DROGON_TEST(UuidTest)
{
    auto uuid = drogon::utils::getUuid();

    std::cout << "uuid: " << uuid << std::endl;

    CHECK(uuid[8] == '-');
    CHECK(uuid[13] == '-');
    CHECK(uuid[18] == '-');
    CHECK(uuid[23] == '-');
    CHECK(uuid.size() == 36);
}
