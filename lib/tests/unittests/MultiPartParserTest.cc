#include <drogon/MultiPart.h>
#include <drogon/drogon_test.h>
#include <drogon/HttpRequest.h>

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
