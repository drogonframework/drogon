#include <string>
#include <string_view>
#include <istream>
#include <array>
#include <vector>
#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>

struct ConvertibleFromStringStream
{
    friend std::istream &operator>>(std::istream &os,
                                    ConvertibleFromStringStream &)
    {
        return os;
    }
};

struct NotConvertibleFromStringStream
{
};

DROGON_TEST(CanConvertFromStringStream)
{
    using drogon::internal::CanConvertFromStringStream;

    static_assert(CanConvertFromStringStream<unsigned short>::value);
    static_assert(CanConvertFromStringStream<unsigned int>::value);
    static_assert(CanConvertFromStringStream<long>::value);
    static_assert(CanConvertFromStringStream<unsigned long>::value);
    static_assert(CanConvertFromStringStream<long long>::value);
    static_assert(CanConvertFromStringStream<unsigned long long>::value);
    static_assert(CanConvertFromStringStream<float>::value);
    static_assert(CanConvertFromStringStream<double>::value);
    static_assert(CanConvertFromStringStream<long double>::value);
    static_assert(CanConvertFromStringStream<bool>::value);
    static_assert(CanConvertFromStringStream<void *>::value);
    static_assert(CanConvertFromStringStream<short>::value);
    static_assert(CanConvertFromStringStream<int>::value);

    static_assert(
        CanConvertFromStringStream<ConvertibleFromStringStream>::value);
    static_assert(
        !CanConvertFromStringStream<NotConvertibleFromStringStream>::value);
}

struct ConstructibleFromString
{
    ConstructibleFromString(const std::string &)
    {
    }
};

struct NotConstructibleFromString
{
};

DROGON_TEST(CanConstructFromString)
{
    using drogon::internal::CanConstructFromString;

    static_assert(CanConstructFromString<ConstructibleFromString>::value);
    static_assert(!CanConstructFromString<NotConstructibleFromString>::value);
    static_assert(!CanConstructFromString<int>::value);
    static_assert(!CanConstructFromString<double>::value);
    static_assert(CanConstructFromString<std::string>::value);
    static_assert(CanConstructFromString<std::string_view>::value);
    static_assert(CanConstructFromString<const std::string>::value);
    static_assert(!CanConstructFromString<std::string &>::value);
    static_assert(CanConstructFromString<const std::string &>::value);
    static_assert(!CanConstructFromString<std::string *>::value);
}

struct ConvertibleFromString
{
    ConvertibleFromString &operator=(const std::string &)
    {
        return *this;
    }
};

struct NotConvertibleFromString
{
};

DROGON_TEST(CanConvertFromString)
{
    using drogon::internal::CanConvertFromString;

    static_assert(CanConvertFromString<ConvertibleFromString>::value);
    static_assert(!CanConvertFromString<NotConvertibleFromString>::value);
    static_assert(CanConvertFromString<std::string>::value);
    static_assert(!CanConvertFromString<const std::string>::value);
    static_assert(!CanConvertFromString<const std::string &>::value);
    static_assert(!CanConvertFromString<std::string *>::value);
    static_assert(CanConvertFromString<std::string_view>::value);
    static_assert(!CanConvertFromString<const char *>::value);
    static_assert(!CanConvertFromString<std::vector<char>>::value);
    static_assert(!CanConvertFromString<std::array<char, 5>>::value);
    static_assert(!CanConvertFromString<int>::value);
    static_assert(!CanConvertFromString<double>::value);
}
