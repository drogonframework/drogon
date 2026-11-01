#include <drogon/MultiPart.h>
#include <drogon/drogon_test.h>
#include <drogon/HttpRequest.h>
#include "../../lib/src/MultipartStreamParser.h"
#include <algorithm>

DROGON_TEST(MultiPartParser)
{
    drogon::MultiPartParser parser1;
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader("content-type", "multipart/form-data; boundary=\"12345\"");
    req->setBody(
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"somekey\"\r\n"
        "\r\n"
        "Hello; World\r\n"
        "--12345--");
    CHECK(0 == parser1.parse(req));
    CHECK(parser1.getParameters().size() == 1);
    CHECK(parser1.getParameters().at("somekey") == "Hello; World");
    req->setBody(
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"somekey\"; "
        "filename=\"test\"\r\n"
        "\r\n"
        "Hello; World\r\n"
        "--12345--");
    drogon::MultiPartParser parser2;
    CHECK(0 == parser2.parse(req));
    auto filesMap = parser2.getFilesMap();
    CHECK(filesMap.size() == 1);
    CHECK(filesMap.at("somekey").getFileName() == "test");
    CHECK(filesMap.at("somekey").fileContent() == "Hello; World");
    req->setBody(
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"name of pdf\"; "
        "filename=\"pdf-file.pdf\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "content-transfer-encoding: quoted-printable\r\n"
        "\r\n"
        "bytes of pdf file\r\n"
        "--12345--");
    drogon::MultiPartParser parser3;
    CHECK(0 == parser3.parse(req));
    filesMap = parser3.getFilesMap();
    CHECK(filesMap.size() == 1);
    CHECK(filesMap.at("name of pdf").getFileName() == "pdf-file.pdf");
    CHECK(filesMap.at("name of pdf").fileContent() == "bytes of pdf file");
    CHECK(filesMap.at("name of pdf").getContentType() ==
          drogon::CT_APPLICATION_OCTET_STREAM);
    CHECK(filesMap.at("name of pdf").getContentTransferEncoding() ==
          "quoted-printable");
    req->setBody(
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"some;key\"\r\n"
        "\r\n"
        "Hello; World\r\n"
        "--12345--");
    drogon::MultiPartParser parser4;
    CHECK(0 == parser4.parse(req));
    CHECK(parser4.getParameters().size() == 1);
    CHECK(parser4.getParameters().at("some;key") == "Hello; World");
}

DROGON_TEST(MultiPartParserDashBoundary)
{
    // Firefox/Gecko style boundary beginning with dashes (RFC 7578):
    // the body delimiter is "--" + boundary, so it starts with many dashes.
    drogon::MultiPartParser parser;
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader(
        "content-type",
        "multipart/form-data; "
        "boundary=\"----geckoformboundary7805dba873e5a74dd0aa7640be4f989\"");
    req->setBody(
        "------geckoformboundary7805dba873e5a74dd0aa7640be4f989\r\n"
        "Content-Disposition: form-data; name=\"key1\"\r\n"
        "\r\n"
        "value1\r\n"
        "------geckoformboundary7805dba873e5a74dd0aa7640be4f989--\r\n");
    CHECK(0 == parser.parse(req));
    CHECK(parser.getParameters().size() == 1);
    CHECK(parser.getParameters().at("key1") == "value1");
}

DROGON_TEST(MultiPartParserRejectsNearbyBoundary)
{
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader("content-type", "multipart/form-data; boundary=A");
    // The second boundary starts only three bytes into the body.
    req->setBody(std::string("A\r\nA") + std::string(100, 'x'));

    drogon::MultiPartParser parser;
    CHECK(parser.parse(req) == -1);

    // A boundary at the end leaves no bytes for the CRLF check.
    req->setBody("A");
    drogon::MultiPartParser parser2;
    CHECK(parser2.parse(req) == -1);
}

DROGON_TEST(MultiPartDispositionParametersAreOrderIndependent)
{
    static const std::string contentType =
        "multipart/form-data; boundary=12345";
    static const std::string body =
        "--12345\r\n"
        "Content-Disposition: form-data; filename=\"file.txt\"; "
        "name=\"upload\"\r\n"
        "\r\n"
        "file content\r\n"
        "--12345--";

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader("content-type", contentType);
    req->setBody(body);

    drogon::MultiPartParser parser;
    CHECK(parser.parse(req) == 0);
    auto files = parser.getFilesMap();
    MANDATE(files.size() == 1);
    CHECK(files.at("upload").getFileName() == "file.txt");

    drogon::MultipartStreamParser streamParser(contentType);
    drogon::MultipartHeader header;
    streamParser.parse(
        body.data(),
        body.size(),
        [&header](drogon::MultipartHeader parsedHeader) {
            header = std::move(parsedHeader);
        },
        [](const char *, size_t) {});
    CHECK(streamParser.isValid());
    CHECK(streamParser.isFinished());
    CHECK(header.name == "upload");
    CHECK(header.filename == "file.txt");
}

DROGON_TEST(MultiPartStreamParser)
{
    static const std::string ct = "multipart/form-data; boundary=\"12345\"";
    static const std::string_view data =
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"key1\"; filename=\"file1\"\r\n"
        "\r\n"
        "Hello; World\r\n"
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"key2\"\r\n"
        "\r\n"
        "value2\r\n"
        "--12345--";

    struct Entry
    {
        drogon::MultipartHeader header;
        std::string value;
        std::string fileContent;
    };

    auto check = [TEST_CTX](size_t step) {
        drogon::MultipartStreamParser parser(ct);

        auto entries = std::make_shared<std::vector<Entry>>();
        auto headerCb = [TEST_CTX, entries](drogon::MultipartHeader hdr) {
            entries->emplace_back(Entry{std::move(hdr)});
        };
        auto dataCb = [TEST_CTX, entries](const char *data, size_t length) {
            MANDATE(!entries->empty());
            if (length == 0)
            {
                // Field finished
                return;
            }
            if (entries->back().header.filename.empty())
            {
                entries->back().value.append(data, length);
            }
            else
            {
                entries->back().fileContent.append(data, length);
            }
        };

        size_t i = 0;
        while (i < data.length() && parser.isValid())
        {
            size_t end = i + step < data.length() ? i + step : data.length();
            parser.parse(data.data() + i, end - i, headerCb, dataCb);
            CHECK(parser.isValid());
            i = end;
        }
        MANDATE(i == data.length());
        MANDATE(parser.isFinished());

        MANDATE(entries->size() == 2);
        CHECK(entries->at(0).header.name == "key1");
        CHECK(entries->at(0).fileContent == "Hello; World");
        CHECK(entries->at(1).header.name == "key2");
        CHECK(entries->at(1).value == "value2");
    };

    check(1);
    check(3);
    check(7);
    check(20);
}

DROGON_TEST(MultiPartStreamParserBoundaryPrefixInBody)
{
    static const std::string body =
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"upload\"; filename=\"x\"\r\n"
        "\r\n"
        "before\r\n--12345Xafter\r\n"
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"empty\"\r\n"
        "\r\n"
        "\r\n--12345--";

    for (size_t step : {1u, 2u, 7u, 32u, 256u})
    {
        drogon::MultipartStreamParser parser(
            "multipart/form-data; boundary=12345");
        std::vector<std::string> parts;
        size_t endCount = 0;
        for (size_t i = 0; i < body.size(); i += step)
        {
            const auto length = (std::min)(step, body.size() - i);
            parser.parse(
                body.data() + i,
                length,
                [&parts](drogon::MultipartHeader) { parts.emplace_back(); },
                [&parts, &endCount](const char *data, size_t length) {
                    if (length == 0)
                    {
                        ++endCount;
                    }
                    else
                    {
                        parts.back().append(data, length);
                    }
                });
            MANDATE(parser.isValid());
        }
        MANDATE(parser.isFinished());
        MANDATE(parts.size() == 2);
        CHECK(parts[0] == "before\r\n--12345Xafter");
        CHECK(parts[1].empty());
        CHECK(endCount == 2);
    }
}

DROGON_TEST(MultiPartBoundaryGrammar)
{
    const std::string body =
        "--Abc\r\n"
        "Content-Disposition: form-data; name=\"upload\"; filename=\"x\"\r\n"
        "\r\n"
        "bare Abc and --AbcX and \r\n--AbcX in a file\r\n"
        "--Abc--";
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setBody(body);

    const std::string validType =
        "multipart/form-data; other=\"x; boundary=wrong\"; BOUNDARY=Abc";
    req->addHeader("content-type", validType);
    drogon::MultiPartParser buffered;
    CHECK(buffered.parse(req) == 0);
    auto files = buffered.getFilesMap();
    MANDATE(files.size() == 1);
    CHECK(files.at("upload").fileContent() ==
          "bare Abc and --AbcX and \r\n--AbcX in a file");

    drogon::MultipartStreamParser stream(validType);
    std::string streamed;
    stream.parse(
        body.data(),
        body.size(),
        [](drogon::MultipartHeader) {},
        [&streamed](const char *data, size_t length) {
            streamed.append(data, length);
        });
    CHECK(stream.isValid());
    CHECK(stream.isFinished());
    CHECK(streamed == files.at("upload").fileContent());

    for (const auto &type : {"multipart/form-data; boundary= Abc",
                             "multipart/form-data; boundary=Abc "})
    {
        drogon::MultipartStreamParser withWhitespace(type);
        CHECK(withWhitespace.isValid());
    }
    for (const auto &type :
         {"multipart/form-data; charset=UTF-8; boundary=Abc",
          "multipart/form-data; charset=UTF-8; boundary=\"Abc\"",
          "Multipart/Form-Data; charset=UTF-8; other=x; BOUNDARY=Abc"})
    {
        req->addHeader("content-type", type);
        drogon::MultiPartParser withExtraParameter;
        CHECK(withExtraParameter.parse(req) == 0);
        drogon::MultipartStreamParser streamedExtra(type);
        CHECK(streamedExtra.isValid());
    }

    const std::vector<std::string> invalidTypes = {
        "multipart/form-data; boundary=",
        "multipart/form-data; boundary=Abc; boundary=Other",
        "multipart/form-data; charset; boundary=Abc",
        "multipart/form-data; boundary=\"bad;one\"",
        "multipart/form-data; boundary=\"Abc\"junk",
        "multipart/form-data; boundary=Abc:Def",
        "multipart/form-data; boundary=" + std::string(71, 'A'),
        "multipart/form-datax; boundary=Abc",
    };
    for (const auto &type : invalidTypes)
    {
        req->addHeader("content-type", type);
        drogon::MultiPartParser rejected;
        CHECK(rejected.parse(req) == -1);
        drogon::MultipartStreamParser rejectedStream(type);
        CHECK(!rejectedStream.isValid());
    }
    drogon::MultipartStreamParser lineBreak(
        "multipart/form-data; boundary=Abc\r\nInjected: x");
    CHECK(!lineBreak.isValid());
    drogon::MultipartStreamParser quotedSpecial(
        "multipart/form-data; boundary=\"Abc:Def\"");
    CHECK(quotedSpecial.isValid());

    req->addHeader("content-type", "multipart/form-data; boundary=Abc");
    req->setBody(body.substr(0, body.size() - 7));  // no closing delimiter
    drogon::MultiPartParser incomplete;
    CHECK(incomplete.parse(req) == -1);
}

DROGON_TEST(MultiPartDispositionEscapedQuote)
{
    // An escaped quote stays inside the name, so the "filename" after it is
    // part of the name value and must not turn the field into a file.
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader("content-type", "multipart/form-data; boundary=12345");
    req->setBody(
        "--12345\r\n"
        "Content-Disposition: form-data; name=\"a\\\"; filename=x\"\r\n"
        "\r\n"
        "value\r\n"
        "--12345--");
    drogon::MultiPartParser parser;
    CHECK(0 == parser.parse(req));
    CHECK(parser.getFiles().empty());
    CHECK(parser.getParameters().size() == 1);
    CHECK(parser.getParameters().count("a\\\"; filename=x") == 1);
}
