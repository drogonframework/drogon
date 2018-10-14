#include <drogon/DrObject.h>
#include <iostream>
namespace api
{
namespace v1
{
template <typename T>
class handler : public drogon::DrObject<T>
{
  public:
    static void p()
    {
        std::cout << handler<T>::classTypeName() << std::endl;
    }
};
class hh : public handler<hh>
{
};
} // namespace v1
} // namespace api
int main()
{
    api::v1::hh h;
    std::cout << h.className() << std::endl;
    std::cout << api::v1::hh::classTypeName() << std::endl;
    h.p();
}