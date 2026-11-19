#include <array>
#include <meta>
#include <string>

#ifndef __cpp_impl_reflection
#error "C++ reflection language support is unavailable"
#endif

#if __cpp_impl_reflection < 202603L
#error "C++ reflection language support is too old"
#endif

#ifndef __cpp_expansion_statements
#error "C++ expansion statements are unavailable"
#endif

#if __cpp_expansion_statements < 202506L
#error "C++ expansion statements support is too old"
#endif

#ifndef __cpp_lib_reflection
#error "C++ reflection library support is unavailable"
#endif

#if __cpp_lib_reflection < 202603L
#error "C++ reflection library support is too old"
#endif

#ifndef __cpp_lib_define_static
#error "Static storage promotion support is unavailable"
#endif

#if __cpp_lib_define_static < 202506L
#error "Static storage promotion support is too old"
#endif

namespace
{
struct Marker
{
    int value;
};

struct [[=Marker{42}]] Annotated
{
    int value;
};

constexpr auto annotatedInfo = ^^Annotated;
constexpr auto annotations =
    std::define_static_array(std::meta::annotations_of(annotatedInfo));
static_assert(annotations.size() == 1);
static_assert(std::meta::extract<Marker>(annotations[0]).value == 42);

constexpr auto staticValues = std::define_static_array(std::array{1, 2, 3});
static_assert(staticValues.size() == 3);
static_assert(staticValues[2] == 3);

constexpr auto staticText =
    std::define_static_string(std::string{"reflection"});
static_assert(std::string_view(staticText) == "reflection");

template <typename T>
consteval int memberCount()
{
    int result = 0;
    template for (constexpr auto member : std::define_static_array(
                      std::meta::nonstatic_data_members_of(
                          ^^T, std::meta::access_context::current())))
    {
        (void)member;
        ++result;
    }
    return result;
}

static_assert(memberCount<Annotated>() == 1);

constexpr auto intInfo = ^^int;
using ReflectedInt = [:intInfo:];
static_assert(__is_same(ReflectedInt, int));
}  // namespace

int main()
{
    return 0;
}
