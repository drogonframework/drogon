#include <drogon/utils/OStringStream.h>
#include <drogon/drogon_test.h>
#include <string>
#include <iostream>

DROGON_TEST(OStringStreamTest)
{
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
