
namespace hpack
{

#define TABLE_ENTRY_SIZE_EXTRA 32
#define HPACK_DYNAMIC_START_INDEX 62
#define HPACK_STATIC_TABLE_SIZE 61
static const HPackTable::KeyValuePair
    hpackStaticTable[HPACK_STATIC_TABLE_SIZE] = {
        std::make_pair(":authority", ""),
        std::make_pair(":method", "GET"),
        std::make_pair(":method", "POST"),
        std::make_pair(":path", "/"),
        std::make_pair(":path", "/index.html"),

        std::make_pair(":scheme", "http"),
        std::make_pair(":scheme", "https"),
        std::make_pair(":status", "200"),
        std::make_pair(":status", "204"),
        std::make_pair(":status", "206"),

        std::make_pair(":status", "304"),
        std::make_pair(":status", "400"),
        std::make_pair(":status", "404"),
        std::make_pair(":status", "500"),
        std::make_pair("accept-charset", ""),

        std::make_pair("accept-encoding", "gzip, deflate"),
        std::make_pair("accept-language", ""),
        std::make_pair("accept-ranges", ""),
        std::make_pair("accept", ""),
        std::make_pair("access-control-allow-origin", ""),

        std::make_pair("age", ""),
        std::make_pair("allow", ""),
        std::make_pair("authorization", ""),
        std::make_pair("cache-control", ""),
        std::make_pair("content-disposition", ""),

        std::make_pair("content-encoding", ""),
        std::make_pair("content-language", ""),
        std::make_pair("content-length", ""),
        std::make_pair("content-location", ""),
        std::make_pair("content-range", ""),

        std::make_pair("content-type", ""),
        std::make_pair("cookie", ""),
        std::make_pair("date", ""),
        std::make_pair("etag", ""),
        std::make_pair("expect", ""),

        std::make_pair("expires", ""),
        std::make_pair("from", ""),
        std::make_pair("host", ""),
        std::make_pair("if-match", ""),
        std::make_pair("if-modified-since", ""),

        std::make_pair("if-none-match", ""),
        std::make_pair("if-range", ""),
        std::make_pair("if-unmodified-since", ""),
        std::make_pair("last-modified", ""),
        std::make_pair("link", ""),

        std::make_pair("location", ""),
        std::make_pair("max-forwards", ""),
        std::make_pair("proxy-authenticate", ""),
        std::make_pair("proxy-authorization", ""),
        std::make_pair("range", ""),

        std::make_pair("referer", ""),
        std::make_pair("refresh", ""),
        std::make_pair("retry-after", ""),
        std::make_pair("server", ""),
        std::make_pair("set-cookie", ""),

        std::make_pair("strict-transport-security", ""),
        std::make_pair("transfer-encoding", ""),
        std::make_pair("user-agent", ""),
        std::make_pair("vary", ""),
        std::make_pair("via", ""),

        std::make_pair("www-authenticate", "")};

}  // namespace hpack
