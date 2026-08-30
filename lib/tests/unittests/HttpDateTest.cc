#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
using namespace drogon;

DROGON_TEST(HttpDate)
{
    // RFC 850
    auto date = utils::getHttpDate("Fri, 05-Jun-20 09:19:38 GMT");
    CHECK(date.microSecondsSinceEpoch() /
              trantor::Date::MICRO_SECONDS_PER_SEC ==
          1591348778);

    // Reddit format
    date = utils::getHttpDate("Fri, 05-Jun-2020 09:19:38 GMT");
    CHECK(date.microSecondsSinceEpoch() /
              trantor::Date::MICRO_SECONDS_PER_SEC ==
          1591348778);

    // Invalid
    date = utils::getHttpDate("Fri, this format is invalid");
    CHECK(date.microSecondsSinceEpoch() == std::numeric_limits<int64_t>::max());

    // ASC Time
    auto epoch = time(nullptr);
    auto str = asctime(gmtime(&epoch));
    date = utils::getHttpDate(str);
    CHECK(date.microSecondsSinceEpoch() /
              trantor::Date::MICRO_SECONDS_PER_SEC ==
          epoch);
}
