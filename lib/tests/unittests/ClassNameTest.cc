#include <drogon/drogon_test.h>

namespace api
{
namespace v1
{
template <typename T>
class handler : public drogon::DrObject<T>
{
  public:
    static std::string name()
    {
        return handler<T>::classTypeName();
    }
};

class hh : public handler<hh>
{
};
}  // namespace v1
}  // namespace api

DROGON_TEST(ClassName)
{
    api::v1::hh h;
    CHECK(h.className() == "api::v1::hh");
    CHECK(api::v1::hh::classTypeName() == "api::v1::hh");
    CHECK(h.name() == "api::v1::hh");
}
