#include <drogon/utils/Utilities.h>
#include <drogon/drogon_test.h>
#include <string>
#include <iostream>
using namespace drogon::utils;
DROGON_TEST(BrotliTest)
{
    SUBSECTION(shortText)
    {
        std::string source{"123中文顶替要枯械"};
        auto compressed = brotliCompress(source.data(), source.length());
        auto decompressed =
            brotliDecompress(compressed.data(), compressed.length());
        CHECK(source == decompressed);
    }

    SUBSECTION(longText)
    {
        std::string source;
        for (size_t i = 0; i < 100000; i++)
        {
            source.append(std::to_string(i));
        }
        auto compressed = brotliCompress(source.data(), source.length());
        auto decompressed =
            brotliDecompress(compressed.data(), compressed.length());
        CHECK(source == decompressed);
    }
}
