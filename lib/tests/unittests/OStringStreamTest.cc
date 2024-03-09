#include <drogon/utils/OStringStream.h>
#include <drogon/drogon_test.h>
#include <string>
#include <string_view>
#include <iostream>

DROGON_TEST(OStringStreamTest)
{
    SUBSECTION(CanConvertToString)
    {
        using drogon::internal::CanConvertToString;

        static_assert(CanConvertToString<int>::value);
        static_assert(CanConvertToString<long>::value);
        static_assert(CanConvertToString<long long>::value);
        static_assert(CanConvertToString<unsigned>::value);
        static_assert(CanConvertToString<unsigned long>::value);
        static_assert(CanConvertToString<unsigned long long>::value);
        static_assert(CanConvertToString<float>::value);
        static_assert(CanConvertToString<double>::value);
        static_assert(CanConvertToString<long double>::value);
        static_assert(!CanConvertToString<std::string>::value);
        static_assert(!CanConvertToString<std::string_view>::value);
    }

    SUBSECTION(integer)
    {
        drogon::OStringStream ss;
        ss << 12;
        ss << 345L;
        CHECK(ss.str() == "12345");
    }

    SUBSECTION(float_number)
    {
        drogon::OStringStream ss;
        ss << 3.14f;
        ss << 3.1416;
        CHECK(ss.str() == "3.143.1416");
    }

    SUBSECTION(literal_string)
    {
        drogon::OStringStream ss;
        ss << "hello";
        ss << " world";
        CHECK(ss.str() == "hello world");
    }

    SUBSECTION(std::string_view)
    {
        drogon::OStringStream ss;
        ss << std::string_view("hello");
        ss << std::string_view(" world");
        CHECK(ss.str() == "hello world");
    }

    SUBSECTION(std_string)
    {
        drogon::OStringStream ss;
        ss << std::string("hello");
        ss << std::string(" world");
        CHECK(ss.str() == "hello world");
    }

    SUBSECTION(mix)
    {
        drogon::OStringStream ss;
        ss << std::string("hello");
        ss << std::string_view(" world");
        ss << "!";
        ss << 123;
        ss << 3.14f;

        CHECK(ss.str() == "hello world!1233.14");
    }
}
