#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>
#include <string_view>

DROGON_TEST(Base64)
{
    constexpr std::string_view in = "drogon framework";
    auto encoded = drogon::utils::base64Encode(in);
    auto decoded = drogon::utils::base64Decode(encoded);
    CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw==");
    CHECK(decoded == in);

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
        auto encoded = drogon::utils::base64EncodeUnpadded(in);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw");
        CHECK(decoded == in);
    }

    SUBSECTION(InPlace12)
    {
        constexpr std::string_view in = "base64encode";
        std::string encoded(drogon::utils::base64EncodedLength(in.size()),
                            '\0');
        std::memcpy(encoded.data(), in.data(), in.size());
        drogon::utils::base64Encode((const unsigned char *)encoded.data(),
                                    in.size(),
                                    (unsigned char *)encoded.data());
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "YmFzZTY0ZW5jb2Rl");
        CHECK(decoded == in);

        // In-place decoding
        encoded.resize(drogon::utils::base64Decode(
            encoded.data(), encoded.size(), (unsigned char *)encoded.data()));
        CHECK(encoded == in);
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
    }

    SUBSECTION(URLSafe)
    {
        auto encoded = drogon::utils::base64Encode(in, true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw==");
        CHECK(decoded == in);
    }

    SUBSECTION(UnpaddedURLSafe)
    {
        auto encoded = drogon::utils::base64EncodeUnpadded(in, true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(encoded == "ZHJvZ29uIGZyYW1ld29yaw");
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
        auto encoded = drogon::utils::base64Encode(in, true);
        auto decoded = drogon::utils::base64Decode(encoded);
        CHECK(decoded == in);
    }
}
