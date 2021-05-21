#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>

DROGON_TEST(Base64)
{
    std::string in{"drogon framework"};
    auto encoded = drogon::utils::base64Encode((const unsigned char *)in.data(),
                                               in.length());
    auto decoded = drogon::utils::base64Decode(encoded);
    CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw==");
    CHECK(decoded == in);

    SUBSECTION(LongString)
    {
        std::string in;
        in.reserve(100000);
        for (int i = 0; i < 100000; ++i)
        {
            in.append(1, char(i));
        }
        auto out = drogon::utils::base64Encode((const unsigned char *)in.data(),
                                               in.length());
        auto out2 = drogon::utils::base64Decode(out);
        auto encoded =
            drogon::utils::base64Encode((const unsigned char *)in.data(),
                                        in.length());
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(decoded == in);
    }

    SUBSECTION(URLSafe)
    {
        std::string in{"drogon framework"};
        auto encoded =
            drogon::utils::base64Encode((const unsigned char *)in.data(),
                                        in.length(),
                                        true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw==");
        CHECK(decoded == in);
    }

    SUBSECTION(LongURLSafe)
    {
        std::string in;
        in.reserve(100000);
        for (int i = 0; i < 100000; ++i)
        {
            in.append(1, char(i));
        }
        auto encoded =
            drogon::utils::base64Encode((const unsigned char *)in.data(),
                                        in.length(),
                                        true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(decoded == in);
    }
}
