#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>

DROGON_TEST(Base64)
{
    std::string in{"drogon framework"};
    auto encoded = drogon::utils::base64Encode(in);
    auto decoded = drogon::utils::base64Decode(encoded);
    CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw==");
    CHECK(decoded == in);
    CHECK(drogon::utils::isBase64(encoded));

    SUBSECTION(InvalidChars)
    {
        auto decoded =
            drogon::utils::base64Decode("ZHJvZ2*9uIGZy**YW1ld2***9yaw*=*=");
        CHECK(decoded == in);
    }

    SUBSECTION(InvalidCharsNoPadding)
    {
        auto decoded =
            drogon::utils::base64Decode("ZHJvZ2*9uIGZy**YW1ld2***9yaw**");
        CHECK(decoded == in);
    }

    SUBSECTION(Unpadded)
    {
        std::string in{"drogon framework"};
        auto encoded = drogon::utils::base64EncodeUnpadded(in);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw");
        CHECK(decoded == in);
        CHECK(drogon::utils::isBase64(encoded));
    }

    SUBSECTION(LongString)
    {
        std::string in;
        in.reserve(100000);
        for (int i = 0; i < 100000; ++i)
        {
            in.append(1, char(i));
        }
        auto out = drogon::utils::base64Encode(in);
        auto out2 = drogon::utils::base64Decode(out);
        auto encoded = drogon::utils::base64Encode(in);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(decoded == in);
        CHECK(out == encoded);
        CHECK(drogon::utils::isBase64(out));
        CHECK(drogon::utils::isBase64(encoded));
    }

    SUBSECTION(URLSafe)
    {
        std::string in{"drogon framework"};
        auto encoded = drogon::utils::base64Encode(in, true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw==");
        CHECK(decoded == in);
        CHECK(drogon::utils::isBase64(encoded));
    }

    SUBSECTION(UnpaddedURLSafe)
    {
        std::string in{"drogon framework"};
        auto encoded = drogon::utils::base64EncodeUnpadded(in, true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw");
        CHECK(decoded == in);
        CHECK(drogon::utils::isBase64(encoded));
    }

    SUBSECTION(LongURLSafe)
    {
        std::string in;
        in.reserve(100000);
        for (int i = 0; i < 100000; ++i)
        {
            in.append(1, char(i));
        }
        auto encoded = drogon::utils::base64Encode(in, true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(decoded == in);
        CHECK(drogon::utils::isBase64(encoded));
    }

    SUBSECTION(emptyString)
    {
        auto encoded = "";
        CHECK(!drogon::utils::isBase64(encoded));
    }

    SUBSECTION(size1Padding)
    {
        auto encoded = "ZHJvZ29uIGZyYW1ld29=";
        CHECK(drogon::utils::isBase64(encoded));
    }

    SUBSECTION(size1PaddingNotModulo4)
    {
        auto encoded = "ZHJvZ29uIGZyYW1ld29ya=";
        CHECK(!drogon::utils::isBase64(encoded));
    }
}
