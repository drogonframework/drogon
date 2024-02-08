#include "../lib/src/HttpUtils.h"
#include <drogon/drogon_test.h>
#include <string>
using namespace drogon;

DROGON_TEST(ExtensionTest)
{
    SUBSECTION(normal)
    {
        std::string str{"drogon.jpg"};
        CHECK(getFileExtension(str) == "jpg");
    }

    SUBSECTION(negative)
    {
        std::string str{"drogon."};
        CHECK(getFileExtension(str) == "");
        str = "drogon";
        CHECK(getFileExtension(str) == "");
        str = "";
        CHECK(getFileExtension(str) == "");
        str = "....";
        CHECK(getFileExtension(str) == "");
    }
}

DROGON_TEST(ContentTypeTest)
{
    SUBSECTION(normal)
    {
        for (int i = CT_NONE + 1; i < CT_CUSTOM; i++)
        {
            auto contentType = ContentType(i);
            const auto mimeType = contentTypeToMime(contentType);
            CHECK(mimeType.empty() == false);
            CHECK(parseContentType(mimeType) == contentType);
            auto exts = getFileExtensions(contentType);
            bool shouldBeEmpty = (contentType == CT_APPLICATION_X_FORM) ||
                                 (contentType == CT_APPLICATION_OCTET_STREAM) ||
                                 (contentType == CT_MULTIPART_FORM_DATA);
            CHECK(exts.empty() == shouldBeEmpty);
            for (const auto &ext : exts)
            {
                auto dummyFile{std::string("dummy.").append(ext)};
                // Handle multiple mime types by setting the default ContentType
                if (contentType == CT_APPLICATION_X_JAVASCRIPT)
                    contentType = CT_TEXT_JAVASCRIPT;
                if (contentType == CT_TEXT_XML)
                    contentType = CT_APPLICATION_XML;
                CHECK(getContentType(dummyFile) == contentType);
                CHECK(parseFileType(dummyFile) != FT_UNKNOWN);
            }
            if (!shouldBeEmpty)
            {
                CHECK(getFileType(contentType) != FT_UNKNOWN);
                CHECK(getFileType(contentType) != FT_CUSTOM);
            }
        }
    }

    SUBSECTION(negative)
    {
        CHECK(getFileType(CT_NONE) == FT_UNKNOWN);
        CHECK(getFileType(CT_CUSTOM) == FT_CUSTOM);
        CHECK(getFileType(CT_APPLICATION_X_FORM) == FT_UNKNOWN);
        CHECK(getFileType(CT_APPLICATION_OCTET_STREAM) == FT_UNKNOWN);
        CHECK(getFileType(CT_MULTIPART_FORM_DATA) == FT_UNKNOWN);
        CHECK(contentTypeToMime(CT_NONE).empty());
        CHECK(contentTypeToMime(CT_CUSTOM) ==
              contentTypeToMime(CT_APPLICATION_OCTET_STREAM));
        CHECK(parseContentType("") == CT_NONE);
        CHECK(parseContentType("application/x-www-form-urlencoded") ==
              CT_APPLICATION_X_FORM);
        CHECK(parseContentType("multipart/form-data") ==
              CT_MULTIPART_FORM_DATA);
        CHECK(parseFileType("any.thing") == FT_CUSTOM);
        CHECK(getContentType("any.thing") == CT_APPLICATION_OCTET_STREAM);
        CHECK(getFileExtensions(CT_NONE).empty());
        CHECK(getFileExtensions(CT_APPLICATION_X_FORM).empty());
        CHECK(getFileExtensions(CT_APPLICATION_OCTET_STREAM).empty());
        CHECK(getFileExtensions(CT_MULTIPART_FORM_DATA).empty());
        CHECK(getFileExtensions(CT_CUSTOM).empty());
    }
}

DROGON_TEST(FileTypeTest)
{
    SUBSECTION(normal)
    {
        CHECK(parseFileType("jpg") == FT_IMAGE);
        CHECK(parseFileType("mp4") == FT_MEDIA);
        CHECK(parseFileType("csp") == FT_CUSTOM);
        CHECK(parseFileType("html") == FT_DOCUMENT);
    }

    SUBSECTION(negative)
    {
        CHECK(parseFileType("") == FT_UNKNOWN);
        CHECK(parseFileType("don'tknow") == FT_CUSTOM);
    }
}
