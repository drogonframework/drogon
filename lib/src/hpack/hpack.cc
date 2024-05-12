#include <stdio.h>
#include <stdint.h>

// cpp header
#include <deque>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>

#include "hpack.h"

// Define of Type Size
typedef int_fast16_t Integer2byte;
typedef uint_fast64_t Uinteger5byte;
// if typedef int_fast16_t change,  INT_FAST16_MAX MUST be change.
const Integer2byte MaxOfInteger2byte = INT_FAST16_MAX;
const unsigned char BitOfUinteger5Byte =
    (sizeof(Uinteger5byte) / sizeof(unsigned char)) * 8;

// need to keep order
const unsigned char IndexedHeaderField = 0x80;
const unsigned char LiteralHeaderFieldWithIncrementalIndexing = 0x40;
const unsigned char DynamicTableSizeUpdate = 0x20;
const unsigned char LiteralHeaderFieldNeverIndexed = 0x10;
const unsigned char LiteralHeaderFieldWithoutIndexing = 0x00;

const Integer2byte SymbolEOS = 256;
const Integer2byte SymbolInitial = 257;
const unsigned char HuffmanLiteralFlag = 0x80;

// default
const size_t SETTINGS_HEADER_TABLE_SIZE_DEFAULT = 4096;

// config
const bool DecodeStringLiteralOctet = true;
const bool HpackDecoderCheckParameter = false;
const bool LiteralDecodeCheckLiteralPad = true;
const bool HpackDecoderTableUpdateHeaderBlank = true;

class HuffmanCode
{
  public:
    const Uinteger5byte msbValue;
    const unsigned char lengthOfBit;

    HuffmanCode(const Uinteger5byte lsbvalue, const unsigned char lengthofbit)
        : msbValue(lsbvalue << (BitOfUinteger5Byte - lengthofbit)),
          lengthOfBit(lengthofbit)
    {
        ;
    }
};

class HuffmanTableColumn
{
  public:
    const bool isSymbol;
    const size_t huffmanTable;
    const Integer2byte symbol;

    HuffmanTableColumn(const bool issymbol,
                       const Integer2byte sym,
                       const size_t huffmantable)
        : isSymbol(issymbol), huffmanTable(huffmantable), symbol(sym)
    {
        ;
    }
};

class HuffmanTable
{
  public:
    const size_t tableAnd;
    const unsigned char thisBit;
    const std::vector<HuffmanTableColumn> table;

    HuffmanTable(const unsigned char thisbit,
                 const unsigned char plusbit,
                 const std::vector<HuffmanTableColumn> &_table)
        : tableAnd(((size_t)1 << plusbit) - 1), thisBit(thisbit), table(_table)
    {
        ;
    }
};

class StaticTableMapElement
{
  public:
    const size_t index;
    const std::string value;

    StaticTableMapElement(const std::string &_value, const size_t _index)
        : index(_index), value(_value)
    {
        ;
    }
};

const size_t StaticTableSize = 62;
const std::pair<const std::string, const std::string>
    StaticTable[StaticTableSize] = {
        std::make_pair<const std::string, const std::string>("TableIndexZero",
                                                             "TableIndexZero"),
        std::make_pair<const std::string, const std::string>(":authority", ""),
        std::make_pair<const std::string, const std::string>(":method", "GET"),
        std::make_pair<const std::string, const std::string>(":method", "POST"),
        std::make_pair<const std::string, const std::string>(":path", "/"),
        std::make_pair<const std::string, const std::string>(":path",
                                                             "/index.html"),
        std::make_pair<const std::string, const std::string>(":scheme", "http"),
        std::make_pair<const std::string, const std::string>(":scheme",
                                                             "https"),
        std::make_pair<const std::string, const std::string>(":status", "200"),
        std::make_pair<const std::string, const std::string>(":status", "204"),
        std::make_pair<const std::string, const std::string>(":status", "206"),
        std::make_pair<const std::string, const std::string>(":status", "304"),
        std::make_pair<const std::string, const std::string>(":status", "400"),
        std::make_pair<const std::string, const std::string>(":status", "404"),
        std::make_pair<const std::string, const std::string>(":status", "500"),
        std::make_pair<const std::string, const std::string>("accept-charset",
                                                             ""),
        std::make_pair<const std::string, const std::string>("accept-encoding",
                                                             "gzip, deflate"),
        std::make_pair<const std::string, const std::string>("accept-language",
                                                             ""),
        std::make_pair<const std::string, const std::string>("accept-ranges",
                                                             ""),
        std::make_pair<const std::string, const std::string>("accept", ""),
        std::make_pair<const std::string, const std::string>(
            "access-control-allow-origin",
            ""),
        std::make_pair<const std::string, const std::string>("age", ""),
        std::make_pair<const std::string, const std::string>("allow", ""),
        std::make_pair<const std::string, const std::string>("authorization",
                                                             ""),
        std::make_pair<const std::string, const std::string>("cache-control",
                                                             ""),
        std::make_pair<const std::string, const std::string>(
            "content-disposition",
            ""),
        std::make_pair<const std::string, const std::string>("content-encoding",
                                                             ""),
        std::make_pair<const std::string, const std::string>("content-language",
                                                             ""),
        std::make_pair<const std::string, const std::string>("content-length",
                                                             ""),
        std::make_pair<const std::string, const std::string>("content-location",
                                                             ""),
        std::make_pair<const std::string, const std::string>("content-range",
                                                             ""),
        std::make_pair<const std::string, const std::string>("content-type",
                                                             ""),
        std::make_pair<const std::string, const std::string>("cookie", ""),
        std::make_pair<const std::string, const std::string>("date", ""),
        std::make_pair<const std::string, const std::string>("etag", ""),
        std::make_pair<const std::string, const std::string>("expect", ""),
        std::make_pair<const std::string, const std::string>("expires", ""),
        std::make_pair<const std::string, const std::string>("from", ""),
        std::make_pair<const std::string, const std::string>("host", ""),
        std::make_pair<const std::string, const std::string>("if-match", ""),
        std::make_pair<const std::string, const std::string>(
            "if-modified-since",
            ""),
        std::make_pair<const std::string, const std::string>("if-none-match",
                                                             ""),
        std::make_pair<const std::string, const std::string>("if-range", ""),
        std::make_pair<const std::string, const std::string>(
            "if-unmodified-since",
            ""),
        std::make_pair<const std::string, const std::string>("last-modified",
                                                             ""),
        std::make_pair<const std::string, const std::string>("link", ""),
        std::make_pair<const std::string, const std::string>("location", ""),
        std::make_pair<const std::string, const std::string>("max-forwards",
                                                             ""),
        std::make_pair<const std::string, const std::string>(
            "proxy-authenticate",
            ""),
        std::make_pair<const std::string, const std::string>(
            "proxy-authorization",
            ""),
        std::make_pair<const std::string, const std::string>("range", ""),
        std::make_pair<const std::string, const std::string>("referer", ""),
        std::make_pair<const std::string, const std::string>("refresh", ""),
        std::make_pair<const std::string, const std::string>("retry-after", ""),
        std::make_pair<const std::string, const std::string>("server", ""),
        std::make_pair<const std::string, const std::string>("set-cookie", ""),
        std::make_pair<const std::string, const std::string>(
            "strict-transport-security",
            ""),
        std::make_pair<const std::string, const std::string>(
            "transfer-encoding",
            ""),
        std::make_pair<const std::string, const std::string>("user-agent", ""),
        std::make_pair<const std::string, const std::string>("vary", ""),
        std::make_pair<const std::string, const std::string>("via", ""),
        std::make_pair<const std::string, const std::string>("www-authenticate",
                                                             ""),
};

const std::unordered_map<std::string, const std::vector<StaticTableMapElement> >
    StaticTableMap = {
        {":authority", {StaticTableMapElement("", 1)}},
        {":method",
         {StaticTableMapElement("GET", 2), StaticTableMapElement("POST", 3)}},
        {":path",
         {StaticTableMapElement("/", 4),
          StaticTableMapElement("/index.html", 5)}},
        {":scheme",
         {StaticTableMapElement("http", 6), StaticTableMapElement("https", 7)}},
        {":status",
         {StaticTableMapElement("206", 10),
          StaticTableMapElement("304", 11),
          StaticTableMapElement("400", 12),
          StaticTableMapElement("404", 13),
          StaticTableMapElement("500", 14),
          StaticTableMapElement("200", 8),
          StaticTableMapElement("204", 9)}},
        {"accept-charset", {StaticTableMapElement("", 15)}},
        {"accept-encoding", {StaticTableMapElement("gzip, deflate", 16)}},
        {"accept-language", {StaticTableMapElement("", 17)}},
        {"accept-ranges", {StaticTableMapElement("", 18)}},
        {"accept", {StaticTableMapElement("", 19)}},
        {"access-control-allow-origin", {StaticTableMapElement("", 20)}},
        {"age", {StaticTableMapElement("", 21)}},
        {"allow", {StaticTableMapElement("", 22)}},
        {"authorization", {StaticTableMapElement("", 23)}},
        {"cache-control", {StaticTableMapElement("", 24)}},
        {"content-disposition", {StaticTableMapElement("", 25)}},
        {"content-encoding", {StaticTableMapElement("", 26)}},
        {"content-language", {StaticTableMapElement("", 27)}},
        {"content-length", {StaticTableMapElement("", 28)}},
        {"content-location", {StaticTableMapElement("", 29)}},
        {"content-range", {StaticTableMapElement("", 30)}},
        {"content-type", {StaticTableMapElement("", 31)}},
        {"cookie", {StaticTableMapElement("", 32)}},
        {"date", {StaticTableMapElement("", 33)}},
        {"etag", {StaticTableMapElement("", 34)}},
        {"expect", {StaticTableMapElement("", 35)}},
        {"expires", {StaticTableMapElement("", 36)}},
        {"from", {StaticTableMapElement("", 37)}},
        {"host", {StaticTableMapElement("", 38)}},
        {"if-match", {StaticTableMapElement("", 39)}},
        {"if-modified-since", {StaticTableMapElement("", 40)}},
        {"if-none-match", {StaticTableMapElement("", 41)}},
        {"if-range", {StaticTableMapElement("", 42)}},
        {"if-unmodified-since", {StaticTableMapElement("", 43)}},
        {"last-modified", {StaticTableMapElement("", 44)}},
        {"link", {StaticTableMapElement("", 45)}},
        {"location", {StaticTableMapElement("", 46)}},
        {"max-forwards", {StaticTableMapElement("", 47)}},
        {"proxy-authenticate", {StaticTableMapElement("", 48)}},
        {"proxy-authorization", {StaticTableMapElement("", 49)}},
        {"range", {StaticTableMapElement("", 50)}},
        {"referer", {StaticTableMapElement("", 51)}},
        {"refresh", {StaticTableMapElement("", 52)}},
        {"retry-after", {StaticTableMapElement("", 53)}},
        {"server", {StaticTableMapElement("", 54)}},
        {"set-cookie", {StaticTableMapElement("", 55)}},
        {"strict-transport-security", {StaticTableMapElement("", 56)}},
        {"transfer-encoding", {StaticTableMapElement("", 57)}},
        {"user-agent", {StaticTableMapElement("", 58)}},
        {"vary", {StaticTableMapElement("", 59)}},
        {"via", {StaticTableMapElement("", 60)}},
        {"www-authenticate", {StaticTableMapElement("", 61)}}};

const HuffmanCode HuffmanCodeTable[257] = {
    HuffmanCode(0x1ff8ul, 13),     HuffmanCode(0x7fffd8ul, 23),
    HuffmanCode(0xfffffe2ul, 28),  HuffmanCode(0xfffffe3ul, 28),
    HuffmanCode(0xfffffe4ul, 28),  HuffmanCode(0xfffffe5ul, 28),
    HuffmanCode(0xfffffe6ul, 28),  HuffmanCode(0xfffffe7ul, 28),
    HuffmanCode(0xfffffe8ul, 28),  HuffmanCode(0xffffeaul, 24),
    HuffmanCode(0x3ffffffcul, 30), HuffmanCode(0xfffffe9ul, 28),
    HuffmanCode(0xfffffeaul, 28),  HuffmanCode(0x3ffffffdul, 30),
    HuffmanCode(0xfffffebul, 28),  HuffmanCode(0xfffffecul, 28),
    HuffmanCode(0xfffffedul, 28),  HuffmanCode(0xfffffeeul, 28),
    HuffmanCode(0xfffffeful, 28),  HuffmanCode(0xffffff0ul, 28),
    HuffmanCode(0xffffff1ul, 28),  HuffmanCode(0xffffff2ul, 28),
    HuffmanCode(0x3ffffffeul, 30), HuffmanCode(0xffffff3ul, 28),
    HuffmanCode(0xffffff4ul, 28),  HuffmanCode(0xffffff5ul, 28),
    HuffmanCode(0xffffff6ul, 28),  HuffmanCode(0xffffff7ul, 28),
    HuffmanCode(0xffffff8ul, 28),  HuffmanCode(0xffffff9ul, 28),
    HuffmanCode(0xffffffaul, 28),  HuffmanCode(0xffffffbul, 28),
    HuffmanCode(0x14ul, 6),        HuffmanCode(0x3f8ul, 10),
    HuffmanCode(0x3f9ul, 10),      HuffmanCode(0xffaul, 12),
    HuffmanCode(0x1ff9ul, 13),     HuffmanCode(0x15ul, 6),
    HuffmanCode(0xf8ul, 8),        HuffmanCode(0x7faul, 11),
    HuffmanCode(0x3faul, 10),      HuffmanCode(0x3fbul, 10),
    HuffmanCode(0xf9ul, 8),        HuffmanCode(0x7fbul, 11),
    HuffmanCode(0xfaul, 8),        HuffmanCode(0x16ul, 6),
    HuffmanCode(0x17ul, 6),        HuffmanCode(0x18ul, 6),
    HuffmanCode(0x0ul, 5),         HuffmanCode(0x1ul, 5),
    HuffmanCode(0x2ul, 5),         HuffmanCode(0x19ul, 6),
    HuffmanCode(0x1aul, 6),        HuffmanCode(0x1bul, 6),
    HuffmanCode(0x1cul, 6),        HuffmanCode(0x1dul, 6),
    HuffmanCode(0x1eul, 6),        HuffmanCode(0x1ful, 6),
    HuffmanCode(0x5cul, 7),        HuffmanCode(0xfbul, 8),
    HuffmanCode(0x7ffcul, 15),     HuffmanCode(0x20ul, 6),
    HuffmanCode(0xffbul, 12),      HuffmanCode(0x3fcul, 10),
    HuffmanCode(0x1ffaul, 13),     HuffmanCode(0x21ul, 6),
    HuffmanCode(0x5dul, 7),        HuffmanCode(0x5eul, 7),
    HuffmanCode(0x5ful, 7),        HuffmanCode(0x60ul, 7),
    HuffmanCode(0x61ul, 7),        HuffmanCode(0x62ul, 7),
    HuffmanCode(0x63ul, 7),        HuffmanCode(0x64ul, 7),
    HuffmanCode(0x65ul, 7),        HuffmanCode(0x66ul, 7),
    HuffmanCode(0x67ul, 7),        HuffmanCode(0x68ul, 7),
    HuffmanCode(0x69ul, 7),        HuffmanCode(0x6aul, 7),
    HuffmanCode(0x6bul, 7),        HuffmanCode(0x6cul, 7),
    HuffmanCode(0x6dul, 7),        HuffmanCode(0x6eul, 7),
    HuffmanCode(0x6ful, 7),        HuffmanCode(0x70ul, 7),
    HuffmanCode(0x71ul, 7),        HuffmanCode(0x72ul, 7),
    HuffmanCode(0xfcul, 8),        HuffmanCode(0x73ul, 7),
    HuffmanCode(0xfdul, 8),        HuffmanCode(0x1ffbul, 13),
    HuffmanCode(0x7fff0ul, 19),    HuffmanCode(0x1ffcul, 13),
    HuffmanCode(0x3ffcul, 14),     HuffmanCode(0x22ul, 6),
    HuffmanCode(0x7ffdul, 15),     HuffmanCode(0x3ul, 5),
    HuffmanCode(0x23ul, 6),        HuffmanCode(0x4ul, 5),
    HuffmanCode(0x24ul, 6),        HuffmanCode(0x5ul, 5),
    HuffmanCode(0x25ul, 6),        HuffmanCode(0x26ul, 6),
    HuffmanCode(0x27ul, 6),        HuffmanCode(0x6ul, 5),
    HuffmanCode(0x74ul, 7),        HuffmanCode(0x75ul, 7),
    HuffmanCode(0x28ul, 6),        HuffmanCode(0x29ul, 6),
    HuffmanCode(0x2aul, 6),        HuffmanCode(0x7ul, 5),
    HuffmanCode(0x2bul, 6),        HuffmanCode(0x76ul, 7),
    HuffmanCode(0x2cul, 6),        HuffmanCode(0x8ul, 5),
    HuffmanCode(0x9ul, 5),         HuffmanCode(0x2dul, 6),
    HuffmanCode(0x77ul, 7),        HuffmanCode(0x78ul, 7),
    HuffmanCode(0x79ul, 7),        HuffmanCode(0x7aul, 7),
    HuffmanCode(0x7bul, 7),        HuffmanCode(0x7ffeul, 15),
    HuffmanCode(0x7fcul, 11),      HuffmanCode(0x3ffdul, 14),
    HuffmanCode(0x1ffdul, 13),     HuffmanCode(0xffffffcul, 28),
    HuffmanCode(0xfffe6ul, 20),    HuffmanCode(0x3fffd2ul, 22),
    HuffmanCode(0xfffe7ul, 20),    HuffmanCode(0xfffe8ul, 20),
    HuffmanCode(0x3fffd3ul, 22),   HuffmanCode(0x3fffd4ul, 22),
    HuffmanCode(0x3fffd5ul, 22),   HuffmanCode(0x7fffd9ul, 23),
    HuffmanCode(0x3fffd6ul, 22),   HuffmanCode(0x7fffdaul, 23),
    HuffmanCode(0x7fffdbul, 23),   HuffmanCode(0x7fffdcul, 23),
    HuffmanCode(0x7fffddul, 23),   HuffmanCode(0x7fffdeul, 23),
    HuffmanCode(0xffffebul, 24),   HuffmanCode(0x7fffdful, 23),
    HuffmanCode(0xffffecul, 24),   HuffmanCode(0xffffedul, 24),
    HuffmanCode(0x3fffd7ul, 22),   HuffmanCode(0x7fffe0ul, 23),
    HuffmanCode(0xffffeeul, 24),   HuffmanCode(0x7fffe1ul, 23),
    HuffmanCode(0x7fffe2ul, 23),   HuffmanCode(0x7fffe3ul, 23),
    HuffmanCode(0x7fffe4ul, 23),   HuffmanCode(0x1fffdcul, 21),
    HuffmanCode(0x3fffd8ul, 22),   HuffmanCode(0x7fffe5ul, 23),
    HuffmanCode(0x3fffd9ul, 22),   HuffmanCode(0x7fffe6ul, 23),
    HuffmanCode(0x7fffe7ul, 23),   HuffmanCode(0xffffeful, 24),
    HuffmanCode(0x3fffdaul, 22),   HuffmanCode(0x1fffddul, 21),
    HuffmanCode(0xfffe9ul, 20),    HuffmanCode(0x3fffdbul, 22),
    HuffmanCode(0x3fffdcul, 22),   HuffmanCode(0x7fffe8ul, 23),
    HuffmanCode(0x7fffe9ul, 23),   HuffmanCode(0x1fffdeul, 21),
    HuffmanCode(0x7fffeaul, 23),   HuffmanCode(0x3fffddul, 22),
    HuffmanCode(0x3fffdeul, 22),   HuffmanCode(0xfffff0ul, 24),
    HuffmanCode(0x1fffdful, 21),   HuffmanCode(0x3fffdful, 22),
    HuffmanCode(0x7fffebul, 23),   HuffmanCode(0x7fffecul, 23),
    HuffmanCode(0x1fffe0ul, 21),   HuffmanCode(0x1fffe1ul, 21),
    HuffmanCode(0x3fffe0ul, 22),   HuffmanCode(0x1fffe2ul, 21),
    HuffmanCode(0x7fffedul, 23),   HuffmanCode(0x3fffe1ul, 22),
    HuffmanCode(0x7fffeeul, 23),   HuffmanCode(0x7fffeful, 23),
    HuffmanCode(0xfffeaul, 20),    HuffmanCode(0x3fffe2ul, 22),
    HuffmanCode(0x3fffe3ul, 22),   HuffmanCode(0x3fffe4ul, 22),
    HuffmanCode(0x7ffff0ul, 23),   HuffmanCode(0x3fffe5ul, 22),
    HuffmanCode(0x3fffe6ul, 22),   HuffmanCode(0x7ffff1ul, 23),
    HuffmanCode(0x3ffffe0ul, 26),  HuffmanCode(0x3ffffe1ul, 26),
    HuffmanCode(0xfffebul, 20),    HuffmanCode(0x7fff1ul, 19),
    HuffmanCode(0x3fffe7ul, 22),   HuffmanCode(0x7ffff2ul, 23),
    HuffmanCode(0x3fffe8ul, 22),   HuffmanCode(0x1ffffecul, 25),
    HuffmanCode(0x3ffffe2ul, 26),  HuffmanCode(0x3ffffe3ul, 26),
    HuffmanCode(0x3ffffe4ul, 26),  HuffmanCode(0x7ffffdeul, 27),
    HuffmanCode(0x7ffffdful, 27),  HuffmanCode(0x3ffffe5ul, 26),
    HuffmanCode(0xfffff1ul, 24),   HuffmanCode(0x1ffffedul, 25),
    HuffmanCode(0x7fff2ul, 19),    HuffmanCode(0x1fffe3ul, 21),
    HuffmanCode(0x3ffffe6ul, 26),  HuffmanCode(0x7ffffe0ul, 27),
    HuffmanCode(0x7ffffe1ul, 27),  HuffmanCode(0x3ffffe7ul, 26),
    HuffmanCode(0x7ffffe2ul, 27),  HuffmanCode(0xfffff2ul, 24),
    HuffmanCode(0x1fffe4ul, 21),   HuffmanCode(0x1fffe5ul, 21),
    HuffmanCode(0x3ffffe8ul, 26),  HuffmanCode(0x3ffffe9ul, 26),
    HuffmanCode(0xffffffdul, 28),  HuffmanCode(0x7ffffe3ul, 27),
    HuffmanCode(0x7ffffe4ul, 27),  HuffmanCode(0x7ffffe5ul, 27),
    HuffmanCode(0xfffecul, 20),    HuffmanCode(0xfffff3ul, 24),
    HuffmanCode(0xfffedul, 20),    HuffmanCode(0x1fffe6ul, 21),
    HuffmanCode(0x3fffe9ul, 22),   HuffmanCode(0x1fffe7ul, 21),
    HuffmanCode(0x1fffe8ul, 21),   HuffmanCode(0x7ffff3ul, 23),
    HuffmanCode(0x3fffeaul, 22),   HuffmanCode(0x3fffebul, 22),
    HuffmanCode(0x1ffffeeul, 25),  HuffmanCode(0x1ffffeful, 25),
    HuffmanCode(0xfffff4ul, 24),   HuffmanCode(0xfffff5ul, 24),
    HuffmanCode(0x3ffffeaul, 26),  HuffmanCode(0x7ffff4ul, 23),
    HuffmanCode(0x3ffffebul, 26),  HuffmanCode(0x7ffffe6ul, 27),
    HuffmanCode(0x3ffffecul, 26),  HuffmanCode(0x3ffffedul, 26),
    HuffmanCode(0x7ffffe7ul, 27),  HuffmanCode(0x7ffffe8ul, 27),
    HuffmanCode(0x7ffffe9ul, 27),  HuffmanCode(0x7ffffeaul, 27),
    HuffmanCode(0x7ffffebul, 27),  HuffmanCode(0xffffffeul, 28),
    HuffmanCode(0x7ffffecul, 27),  HuffmanCode(0x7ffffedul, 27),
    HuffmanCode(0x7ffffeeul, 27),  HuffmanCode(0x7ffffeful, 27),
    HuffmanCode(0x7fffff0ul, 27),  HuffmanCode(0x3ffffeeul, 26),
    HuffmanCode(0x3ffffffful, 30),
};

const size_t HuffmanDecodeTableSize = 80;
const HuffmanTable HuffmanDecodeTable[HuffmanDecodeTableSize] = {
    HuffmanTable(5,
                 5,
                 {
                     HuffmanTableColumn(true, 48, -1),
                     HuffmanTableColumn(true, 49, -1),
                     HuffmanTableColumn(true, 50, -1),
                     HuffmanTableColumn(true, 97, -1),
                     HuffmanTableColumn(true, 99, -1),
                     HuffmanTableColumn(true, 101, -1),
                     HuffmanTableColumn(true, 105, -1),
                     HuffmanTableColumn(true, 111, -1),
                     HuffmanTableColumn(true, 115, -1),
                     HuffmanTableColumn(true, 116, -1),
                     HuffmanTableColumn(false, SymbolInitial, 1),
                     HuffmanTableColumn(false, SymbolInitial, 2),
                     HuffmanTableColumn(false, SymbolInitial, 3),
                     HuffmanTableColumn(false, SymbolInitial, 4),
                     HuffmanTableColumn(false, SymbolInitial, 5),
                     HuffmanTableColumn(false, SymbolInitial, 6),
                     HuffmanTableColumn(false, SymbolInitial, 7),
                     HuffmanTableColumn(false, SymbolInitial, 8),
                     HuffmanTableColumn(false, SymbolInitial, 9),
                     HuffmanTableColumn(false, SymbolInitial, 10),
                     HuffmanTableColumn(false, SymbolInitial, 11),
                     HuffmanTableColumn(false, SymbolInitial, 12),
                     HuffmanTableColumn(false, SymbolInitial, 13),
                     HuffmanTableColumn(false, SymbolInitial, 14),
                     HuffmanTableColumn(false, SymbolInitial, 15),
                     HuffmanTableColumn(false, SymbolInitial, 16),
                     HuffmanTableColumn(false, SymbolInitial, 17),
                     HuffmanTableColumn(false, SymbolInitial, 18),
                     HuffmanTableColumn(false, SymbolInitial, 19),
                     HuffmanTableColumn(false, SymbolInitial, 20),
                     HuffmanTableColumn(false, SymbolInitial, 21),
                     HuffmanTableColumn(false, SymbolInitial, 22),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 32, -1),
                     HuffmanTableColumn(true, 37, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 45, -1),
                     HuffmanTableColumn(true, 46, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 47, -1),
                     HuffmanTableColumn(true, 51, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 52, -1),
                     HuffmanTableColumn(true, 53, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 54, -1),
                     HuffmanTableColumn(true, 55, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 56, -1),
                     HuffmanTableColumn(true, 57, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 61, -1),
                     HuffmanTableColumn(true, 65, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 95, -1),
                     HuffmanTableColumn(true, 98, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 100, -1),
                     HuffmanTableColumn(true, 102, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 103, -1),
                     HuffmanTableColumn(true, 104, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 108, -1),
                     HuffmanTableColumn(true, 109, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 110, -1),
                     HuffmanTableColumn(true, 112, -1),
                 }),
    HuffmanTable(6,
                 1,
                 {
                     HuffmanTableColumn(true, 114, -1),
                     HuffmanTableColumn(true, 117, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 58, -1),
                     HuffmanTableColumn(true, 66, -1),
                     HuffmanTableColumn(true, 67, -1),
                     HuffmanTableColumn(true, 68, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 69, -1),
                     HuffmanTableColumn(true, 70, -1),
                     HuffmanTableColumn(true, 71, -1),
                     HuffmanTableColumn(true, 72, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 73, -1),
                     HuffmanTableColumn(true, 74, -1),
                     HuffmanTableColumn(true, 75, -1),
                     HuffmanTableColumn(true, 76, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 77, -1),
                     HuffmanTableColumn(true, 78, -1),
                     HuffmanTableColumn(true, 79, -1),
                     HuffmanTableColumn(true, 80, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 81, -1),
                     HuffmanTableColumn(true, 82, -1),
                     HuffmanTableColumn(true, 83, -1),
                     HuffmanTableColumn(true, 84, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 85, -1),
                     HuffmanTableColumn(true, 86, -1),
                     HuffmanTableColumn(true, 87, -1),
                     HuffmanTableColumn(true, 89, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 106, -1),
                     HuffmanTableColumn(true, 107, -1),
                     HuffmanTableColumn(true, 113, -1),
                     HuffmanTableColumn(true, 118, -1),
                 }),
    HuffmanTable(7,
                 2,
                 {
                     HuffmanTableColumn(true, 119, -1),
                     HuffmanTableColumn(true, 120, -1),
                     HuffmanTableColumn(true, 121, -1),
                     HuffmanTableColumn(true, 122, -1),
                 }),
    HuffmanTable(8,
                 3,
                 {
                     HuffmanTableColumn(true, 38, -1),
                     HuffmanTableColumn(true, 42, -1),
                     HuffmanTableColumn(true, 44, -1),
                     HuffmanTableColumn(true, 59, -1),
                     HuffmanTableColumn(true, 88, -1),
                     HuffmanTableColumn(true, 90, -1),
                     HuffmanTableColumn(false, SymbolInitial, 23),
                     HuffmanTableColumn(false, SymbolInitial, 24),
                 }),
    HuffmanTable(10,
                 2,
                 {
                     HuffmanTableColumn(true, 33, -1),
                     HuffmanTableColumn(true, 34, -1),
                     HuffmanTableColumn(true, 40, -1),
                     HuffmanTableColumn(true, 41, -1),
                 }),
    HuffmanTable(10,
                 2,
                 {
                     HuffmanTableColumn(true, 63, -1),
                     HuffmanTableColumn(false, SymbolInitial, 25),
                     HuffmanTableColumn(false, SymbolInitial, 26),
                     HuffmanTableColumn(false, SymbolInitial, 28),
                 }),
    HuffmanTable(11,
                 1,
                 {
                     HuffmanTableColumn(true, 39, -1),
                     HuffmanTableColumn(true, 43, -1),
                 }),
    HuffmanTable(11,
                 1,
                 {
                     HuffmanTableColumn(true, 124, -1),
                     HuffmanTableColumn(false, SymbolInitial, 27),
                 }),
    HuffmanTable(12,
                 1,
                 {
                     HuffmanTableColumn(true, 35, -1),
                     HuffmanTableColumn(true, 62, -1),
                 }),
    HuffmanTable(13,
                 3,
                 {
                     HuffmanTableColumn(true, 0, -1),
                     HuffmanTableColumn(true, 36, -1),
                     HuffmanTableColumn(true, 64, -1),
                     HuffmanTableColumn(true, 91, -1),
                     HuffmanTableColumn(true, 93, -1),
                     HuffmanTableColumn(true, 126, -1),
                     HuffmanTableColumn(false, SymbolInitial, 29),
                     HuffmanTableColumn(false, SymbolInitial, 30),
                 }),
    HuffmanTable(14,
                 1,
                 {
                     HuffmanTableColumn(true, 94, -1),
                     HuffmanTableColumn(true, 125, -1),
                 }),
    HuffmanTable(15,
                 2,
                 {
                     HuffmanTableColumn(true, 60, -1),
                     HuffmanTableColumn(true, 96, -1),
                     HuffmanTableColumn(true, 123, -1),
                     HuffmanTableColumn(false, SymbolInitial, 31),
                 }),
    HuffmanTable(19,
                 4,
                 {
                     HuffmanTableColumn(true, 92, -1),
                     HuffmanTableColumn(true, 195, -1),
                     HuffmanTableColumn(true, 208, -1),
                     HuffmanTableColumn(false, SymbolInitial, 32),
                     HuffmanTableColumn(false, SymbolInitial, 33),
                     HuffmanTableColumn(false, SymbolInitial, 34),
                     HuffmanTableColumn(false, SymbolInitial, 35),
                     HuffmanTableColumn(false, SymbolInitial, 36),
                     HuffmanTableColumn(false, SymbolInitial, 37),
                     HuffmanTableColumn(false, SymbolInitial, 38),
                     HuffmanTableColumn(false, SymbolInitial, 39),
                     HuffmanTableColumn(false, SymbolInitial, 43),
                     HuffmanTableColumn(false, SymbolInitial, 44),
                     HuffmanTableColumn(false, SymbolInitial, 45),
                     HuffmanTableColumn(false, SymbolInitial, 50),
                     HuffmanTableColumn(false, SymbolInitial, 51),
                 }),
    HuffmanTable(20,
                 1,
                 {
                     HuffmanTableColumn(true, 128, -1),
                     HuffmanTableColumn(true, 130, -1),
                 }),
    HuffmanTable(20,
                 1,
                 {
                     HuffmanTableColumn(true, 131, -1),
                     HuffmanTableColumn(true, 162, -1),
                 }),
    HuffmanTable(20,
                 1,
                 {
                     HuffmanTableColumn(true, 184, -1),
                     HuffmanTableColumn(true, 194, -1),
                 }),
    HuffmanTable(20,
                 1,
                 {
                     HuffmanTableColumn(true, 224, -1),
                     HuffmanTableColumn(true, 226, -1),
                 }),
    HuffmanTable(21,
                 2,
                 {
                     HuffmanTableColumn(true, 153, -1),
                     HuffmanTableColumn(true, 161, -1),
                     HuffmanTableColumn(true, 167, -1),
                     HuffmanTableColumn(true, 172, -1),
                 }),
    HuffmanTable(21,
                 2,
                 {
                     HuffmanTableColumn(true, 176, -1),
                     HuffmanTableColumn(true, 177, -1),
                     HuffmanTableColumn(true, 179, -1),
                     HuffmanTableColumn(true, 209, -1),
                 }),
    HuffmanTable(21,
                 2,
                 {
                     HuffmanTableColumn(true, 216, -1),
                     HuffmanTableColumn(true, 217, -1),
                     HuffmanTableColumn(true, 227, -1),
                     HuffmanTableColumn(true, 229, -1),
                 }),
    HuffmanTable(21,
                 2,
                 {
                     HuffmanTableColumn(true, 230, -1),
                     HuffmanTableColumn(false, SymbolInitial, 40),
                     HuffmanTableColumn(false, SymbolInitial, 41),
                     HuffmanTableColumn(false, SymbolInitial, 42),
                 }),
    HuffmanTable(22,
                 1,
                 {
                     HuffmanTableColumn(true, 129, -1),
                     HuffmanTableColumn(true, 132, -1),
                 }),
    HuffmanTable(22,
                 1,
                 {
                     HuffmanTableColumn(true, 133, -1),
                     HuffmanTableColumn(true, 134, -1),
                 }),
    HuffmanTable(22,
                 1,
                 {
                     HuffmanTableColumn(true, 136, -1),
                     HuffmanTableColumn(true, 146, -1),
                 }),
    HuffmanTable(22,
                 3,
                 {
                     HuffmanTableColumn(true, 154, -1),
                     HuffmanTableColumn(true, 156, -1),
                     HuffmanTableColumn(true, 160, -1),
                     HuffmanTableColumn(true, 163, -1),
                     HuffmanTableColumn(true, 164, -1),
                     HuffmanTableColumn(true, 169, -1),
                     HuffmanTableColumn(true, 170, -1),
                     HuffmanTableColumn(true, 173, -1),
                 }),
    HuffmanTable(22,
                 3,
                 {
                     HuffmanTableColumn(true, 178, -1),
                     HuffmanTableColumn(true, 181, -1),
                     HuffmanTableColumn(true, 185, -1),
                     HuffmanTableColumn(true, 186, -1),
                     HuffmanTableColumn(true, 187, -1),
                     HuffmanTableColumn(true, 189, -1),
                     HuffmanTableColumn(true, 190, -1),
                     HuffmanTableColumn(true, 196, -1),
                 }),
    HuffmanTable(22,
                 3,
                 {
                     HuffmanTableColumn(true, 198, -1),
                     HuffmanTableColumn(true, 228, -1),
                     HuffmanTableColumn(true, 232, -1),
                     HuffmanTableColumn(true, 233, -1),
                     HuffmanTableColumn(false, SymbolInitial, 46),
                     HuffmanTableColumn(false, SymbolInitial, 47),
                     HuffmanTableColumn(false, SymbolInitial, 48),
                     HuffmanTableColumn(false, SymbolInitial, 49),
                 }),
    HuffmanTable(23,
                 1,
                 {
                     HuffmanTableColumn(true, 1, -1),
                     HuffmanTableColumn(true, 135, -1),
                 }),
    HuffmanTable(23,
                 1,
                 {
                     HuffmanTableColumn(true, 137, -1),
                     HuffmanTableColumn(true, 138, -1),
                 }),
    HuffmanTable(23,
                 1,
                 {
                     HuffmanTableColumn(true, 139, -1),
                     HuffmanTableColumn(true, 140, -1),
                 }),
    HuffmanTable(23,
                 1,
                 {
                     HuffmanTableColumn(true, 141, -1),
                     HuffmanTableColumn(true, 143, -1),
                 }),
    HuffmanTable(23,
                 4,
                 {
                     HuffmanTableColumn(true, 147, -1),
                     HuffmanTableColumn(true, 149, -1),
                     HuffmanTableColumn(true, 150, -1),
                     HuffmanTableColumn(true, 151, -1),
                     HuffmanTableColumn(true, 152, -1),
                     HuffmanTableColumn(true, 155, -1),
                     HuffmanTableColumn(true, 157, -1),
                     HuffmanTableColumn(true, 158, -1),
                     HuffmanTableColumn(true, 165, -1),
                     HuffmanTableColumn(true, 166, -1),
                     HuffmanTableColumn(true, 168, -1),
                     HuffmanTableColumn(true, 174, -1),
                     HuffmanTableColumn(true, 175, -1),
                     HuffmanTableColumn(true, 180, -1),
                     HuffmanTableColumn(true, 182, -1),
                     HuffmanTableColumn(true, 183, -1),
                 }),
    HuffmanTable(23,
                 4,
                 {
                     HuffmanTableColumn(true, 188, -1),
                     HuffmanTableColumn(true, 191, -1),
                     HuffmanTableColumn(true, 197, -1),
                     HuffmanTableColumn(true, 231, -1),
                     HuffmanTableColumn(true, 239, -1),
                     HuffmanTableColumn(false, SymbolInitial, 52),
                     HuffmanTableColumn(false, SymbolInitial, 53),
                     HuffmanTableColumn(false, SymbolInitial, 54),
                     HuffmanTableColumn(false, SymbolInitial, 55),
                     HuffmanTableColumn(false, SymbolInitial, 56),
                     HuffmanTableColumn(false, SymbolInitial, 57),
                     HuffmanTableColumn(false, SymbolInitial, 58),
                     HuffmanTableColumn(false, SymbolInitial, 59),
                     HuffmanTableColumn(false, SymbolInitial, 60),
                     HuffmanTableColumn(false, SymbolInitial, 62),
                     HuffmanTableColumn(false, SymbolInitial, 63),
                 }),
    HuffmanTable(24,
                 1,
                 {
                     HuffmanTableColumn(true, 9, -1),
                     HuffmanTableColumn(true, 142, -1),
                 }),
    HuffmanTable(24,
                 1,
                 {
                     HuffmanTableColumn(true, 144, -1),
                     HuffmanTableColumn(true, 145, -1),
                 }),
    HuffmanTable(24,
                 1,
                 {
                     HuffmanTableColumn(true, 148, -1),
                     HuffmanTableColumn(true, 159, -1),
                 }),
    HuffmanTable(24,
                 1,
                 {
                     HuffmanTableColumn(true, 171, -1),
                     HuffmanTableColumn(true, 206, -1),
                 }),
    HuffmanTable(24,
                 1,
                 {
                     HuffmanTableColumn(true, 215, -1),
                     HuffmanTableColumn(true, 225, -1),
                 }),
    HuffmanTable(24,
                 1,
                 {
                     HuffmanTableColumn(true, 236, -1),
                     HuffmanTableColumn(true, 237, -1),
                 }),
    HuffmanTable(25,
                 2,
                 {
                     HuffmanTableColumn(true, 199, -1),
                     HuffmanTableColumn(true, 207, -1),
                     HuffmanTableColumn(true, 234, -1),
                     HuffmanTableColumn(true, 235, -1),
                 }),
    HuffmanTable(26,
                 3,
                 {
                     HuffmanTableColumn(true, 192, -1),
                     HuffmanTableColumn(true, 193, -1),
                     HuffmanTableColumn(true, 200, -1),
                     HuffmanTableColumn(true, 201, -1),
                     HuffmanTableColumn(true, 202, -1),
                     HuffmanTableColumn(true, 205, -1),
                     HuffmanTableColumn(true, 210, -1),
                     HuffmanTableColumn(true, 213, -1),
                 }),
    HuffmanTable(26,
                 3,
                 {
                     HuffmanTableColumn(true, 218, -1),
                     HuffmanTableColumn(true, 219, -1),
                     HuffmanTableColumn(true, 238, -1),
                     HuffmanTableColumn(true, 240, -1),
                     HuffmanTableColumn(true, 242, -1),
                     HuffmanTableColumn(true, 243, -1),
                     HuffmanTableColumn(true, 255, -1),
                     HuffmanTableColumn(false, SymbolInitial, 61),
                 }),
    HuffmanTable(27,
                 1,
                 {
                     HuffmanTableColumn(true, 203, -1),
                     HuffmanTableColumn(true, 204, -1),
                 }),
    HuffmanTable(27,
                 4,
                 {
                     HuffmanTableColumn(true, 211, -1),
                     HuffmanTableColumn(true, 212, -1),
                     HuffmanTableColumn(true, 214, -1),
                     HuffmanTableColumn(true, 221, -1),
                     HuffmanTableColumn(true, 222, -1),
                     HuffmanTableColumn(true, 223, -1),
                     HuffmanTableColumn(true, 241, -1),
                     HuffmanTableColumn(true, 244, -1),
                     HuffmanTableColumn(true, 245, -1),
                     HuffmanTableColumn(true, 246, -1),
                     HuffmanTableColumn(true, 247, -1),
                     HuffmanTableColumn(true, 248, -1),
                     HuffmanTableColumn(true, 250, -1),
                     HuffmanTableColumn(true, 251, -1),
                     HuffmanTableColumn(true, 252, -1),
                     HuffmanTableColumn(true, 253, -1),
                 }),
    HuffmanTable(27,
                 4,
                 {
                     HuffmanTableColumn(true, 254, -1),
                     HuffmanTableColumn(false, SymbolInitial, 64),
                     HuffmanTableColumn(false, SymbolInitial, 65),
                     HuffmanTableColumn(false, SymbolInitial, 66),
                     HuffmanTableColumn(false, SymbolInitial, 67),
                     HuffmanTableColumn(false, SymbolInitial, 68),
                     HuffmanTableColumn(false, SymbolInitial, 69),
                     HuffmanTableColumn(false, SymbolInitial, 70),
                     HuffmanTableColumn(false, SymbolInitial, 71),
                     HuffmanTableColumn(false, SymbolInitial, 72),
                     HuffmanTableColumn(false, SymbolInitial, 73),
                     HuffmanTableColumn(false, SymbolInitial, 74),
                     HuffmanTableColumn(false, SymbolInitial, 75),
                     HuffmanTableColumn(false, SymbolInitial, 76),
                     HuffmanTableColumn(false, SymbolInitial, 77),
                     HuffmanTableColumn(false, SymbolInitial, 78),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 2, -1),
                     HuffmanTableColumn(true, 3, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 4, -1),
                     HuffmanTableColumn(true, 5, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 6, -1),
                     HuffmanTableColumn(true, 7, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 8, -1),
                     HuffmanTableColumn(true, 11, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 12, -1),
                     HuffmanTableColumn(true, 14, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 15, -1),
                     HuffmanTableColumn(true, 16, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 17, -1),
                     HuffmanTableColumn(true, 18, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 19, -1),
                     HuffmanTableColumn(true, 20, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 21, -1),
                     HuffmanTableColumn(true, 23, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 24, -1),
                     HuffmanTableColumn(true, 25, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 26, -1),
                     HuffmanTableColumn(true, 27, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 28, -1),
                     HuffmanTableColumn(true, 29, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 30, -1),
                     HuffmanTableColumn(true, 31, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 127, -1),
                     HuffmanTableColumn(true, 220, -1),
                 }),
    HuffmanTable(28,
                 1,
                 {
                     HuffmanTableColumn(true, 249, -1),
                     HuffmanTableColumn(false, SymbolInitial, 79),
                 }),
    HuffmanTable(30,
                 2,
                 {
                     HuffmanTableColumn(true, 10, -1),
                     HuffmanTableColumn(true, 13, -1),
                     HuffmanTableColumn(true, 22, -1),
                     HuffmanTableColumn(true, 256, -1),
                 }),
};

#if DEBUGFLAG == 1
char *LsbByte(unsigned char ii)
{
    static char rlt[9];
    unsigned char bcmp = 1;
    for (Integer2byte i = 0; i < 8; ++i)
    {
        rlt[7 - i] = (bcmp & ii) == 0 ? '0' : '1';
        bcmp = bcmp << 1;
    }
    rlt[8] = '\0';
    return rlt;
}

void KeyValPairPrint(const KeyValPair &kvp)
{
    printf("%s:%s\n", kvp.first.c_str(), kvp.second.c_str());
    return;
}
#endif  // DEBUGFLAG

#define XXXERRORCHECKUNEXPECTSTAT(stat)                    \
    {                                                      \
        printf("%s:%d XXXErrorCheckUnexpectStatus(%s).\n", \
               __FILE__,                                   \
               __LINE__,                                   \
               HpackStatus2CString((stat)));               \
        return (stat);                                     \
    }

inline const char *HpackStatus2CString(const HpackStatus stat)
{
    switch (stat)
    {
        case HpackStatus::Success:
            return "HpackStatus::Success";
        case HpackStatus::IntegerOverFlow:
            return "HpackStatus::IntegerOverFlow";
        case HpackStatus::RBuffEndAtIntegerDecode:
            return "HpackStatus::RBuffEndAtIntegerDecode";
        case HpackStatus::TableIndexZero:
            return "HpackStatus::TableIndexZero";
        case HpackStatus::TableIndeNotExist:
            return "HpackStatus::TableIndeNotExist";
        case HpackStatus::StringLiteralOctet:
            return "HpackStatus::StringLiteralOctet";
        case HpackStatus::LiteralPadNotEOSMsb:
            return "HpackStatus::LiteralPadNotEOSMsb";
        case HpackStatus::LiteralContainEOS:
            return "HpackStatus::LiteralContainEOS";
        case HpackStatus::LiteralPadLongThan7Bit:
            return "HpackStatus::LiteralPadLongThan7Bit";
        case HpackStatus::RBuffEndAtLiteralDecode:
            return "HpackStatus::RBuffEndAtLiteralDecode";
        case HpackStatus::RBuffEndAtHeaderField:
            return "HpackStatus::RBuffEndAtHeaderField";
        case HpackStatus::RBuffZeroLength:
            return "HpackStatus::RBuffZeroLength";
        case HpackStatus::WBuffErrIntegerEecode:
            return "HpackStatus::WBuffErrIntegerEecode";
        case HpackStatus::WBuffErrLiteralEecode:
            return "HpackStatus::WBuffErrLiteralEecode";
        case HpackStatus::WBuffErrHeaderField:
            return "HpackStatus::WBuffErrHeaderField";
        case HpackStatus::HpackEncodeFieldRepresent:
            return "HpackStatus::HpackEncodeFieldRepresent";
        case HpackStatus::DecodeSwitchEnd:
            return "HpackStatus::DecodeSwitchEnd";
        case HpackStatus::HfdiListLength:
            return "HpackStatus::HfdiListLength";
        case HpackStatus::LiteralEncodeWay:
            return "HpackStatus::LiteralEncodeWay";
        case HpackStatus::TableSizeOverflow:
            return "HpackStatus::TableSizeOverflow";
        default:
            return "Unexpect status in HpackStatus2String.";
    }
}

inline bool HeaderFieldInfoEqual(const HeaderFieldInfo &lhs,
                                 const HeaderFieldInfo &rhs) noexcept
{
    if (lhs.evictCounter != rhs.evictCounter)
    {
        return false;
    }
    if (lhs.representation != rhs.representation)
    {
        return false;
    }
    return true;
}

bool HeaderFieldInfo::operator==(const HeaderFieldInfo &rhs) const noexcept
{
    return HeaderFieldInfoEqual(*this, rhs);
}

bool HeaderFieldInfo::operator!=(const HeaderFieldInfo &rhs) const noexcept
{
    return !HeaderFieldInfoEqual(*this, rhs);
}

class VecHpRBuffer : public HpRBuffer
{
  public:
    size_t pos;
    const std::vector<unsigned char> &ss;

    VecHpRBuffer(const std::vector<unsigned char> &ss) : pos(0), ss(ss)
    {
        ;
    }

    unsigned char Current() override
    {
        return ss[pos];
    }

    bool Next() override
    {
        if (pos < ss.size() - 1)
        {
            pos += 1;
            return true;
        }
        return false;
    }

    size_t Size() override
    {
        return ss.size();
    }
};

class ArrHpRBuffer : public HpRBuffer
{
  public:
    size_t size;
    const unsigned char *ptr;
    const unsigned char *end;

    ArrHpRBuffer(const unsigned char *start, size_t sizebyte)
        : size(sizebyte),
          ptr(start),
          end(start + ((sizebyte / sizeof(unsigned char)) - 1))
    {
        ;
    }

    bool Next() override
    {
        if (ptr == end)
        {
            return false;
        }
        ptr += 1;
        return true;
    }

    size_t Size() override
    {
        return size;
    }

    unsigned char Current() override
    {
        return *ptr;
    }

    /*
    const ArrHpRBuffer& operator = (const ArrHpRBuffer &r){
        ptr = r.ptr;
        end = r.end;
        size = r.size;
        return *this;
    }*/
};

class VecHpWBuffer : public HpWBuffer
{
  public:
    size_t maxSize;
    std::vector<unsigned char> &s;

    VecHpWBuffer(std::vector<unsigned char> &ss) : maxSize(8192), s(ss)
    {
        ;
    }

    VecHpWBuffer(std::vector<unsigned char> &ss, size_t maxsize)
        : maxSize(maxsize), s(ss)
    {
        ;
    }

    unsigned char OrCurrent(unsigned char i) override
    {
        if (s.size() == 0)
        {
            s.push_back(i);
        }
        else
        {
            s[(s.size() - 1)] = (s[(s.size() - 1)] | i);
        }
        return s[s.size() - 1];
    }

    bool Append(unsigned char i) override
    {
        if (s.size() <= maxSize)
        {
            s.push_back(i);
            return true;
        }
        return false;
    }

    size_t Size() override
    {
        return s.size();
    }
};

#if DEBUGFLAG == 1
void BufferPrint(const std::vector<unsigned char> &s)
{
    size_t i;
    printf("Size%3zu\n", s.size());
    for (i = 0; i < s.size(); ++i)
    {
        printf("%s ", LsbByte(s[i]));
        if ((i + 1) % 4 == 0)
        {
            printf("\n");
        }
    }
    if ((i) % 4 != 0)
    {
        printf("\n");
    }
}

void BufferFromHex(const std::string &str, std::vector<unsigned char> &s)
{
    bool firstc = true;
    unsigned char tmp = 0;
    for (char c : str)
    {
        if (c == ' ')
        {
            continue;
        }
        else if (c >= '0' && c <= '9')
        {
            tmp += c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            tmp += (c - 'a') + 10;
        }
        else if (c >= 'A' && c <= 'F')
        {
            tmp += (c - 'A') + 10;
        }
        else
        {
            printf("BufferFromHex Unexpect char %d.\n", (int)c);
            continue;
        }
        if (firstc)
        {
            tmp = tmp << 4;
            firstc = false;
        }
        else
        {
            firstc = true;
            s.push_back(tmp);
            tmp = 0;
        }
    }
    if (!firstc)
    {
        printf("BufferFromHex Hexstr must be an even.\n");
    }
}

std::string _String2Hex(const std::string &s)
{
    std::string r;
    const unsigned char trtable[17] = "0123456789abcdef";
    for (const unsigned char c : s)
    {
        r.append(1, trtable[(c & 0xf0) >> 4]);
        r.append(1, trtable[c & 0x0f]);
    }
    return r;
}
#endif  // DEBUGFLAG

inline HpackStatus IntegerEncode(HpWBuffer &w,
                                 const Integer2byte prefix,
                                 Uinteger5byte i)
{
#if DEBUGFLAG == 1
    if (prefix > 8)
    {
        printf("IntegerEncode prefix(%jd) only supper less then 8.",
               (intmax_t)prefix);
    }
#endif  // DEBUGFLAG

    Uinteger5byte tmp;
    if (i >= (1U << (Uinteger5byte)prefix) - 1)
    {
        w.OrCurrent((1 << prefix) - 1);
        i = i - ((1 << prefix) - 1);
        do
        {
            tmp = i & 0x007f;
            i = i >> 7;
            if (i > 0)
            {
                tmp = tmp | 0x0080;
            }
            if (!w.Append((unsigned char)tmp))
            {
                return HpackStatus::WBuffErrIntegerEecode;
            }
        } while (i != 0);
    }
    else
    {
        w.OrCurrent((unsigned char)i);
    }
    return HpackStatus::Success;
}

#define INTEGERENCODEERRORCHECK(stat)            \
    switch (stat)                                \
    {                                            \
        case HpackStatus::Success:               \
            break;                               \
        case HpackStatus::WBuffErrIntegerEecode: \
            return stat;                         \
        default:                                 \
            XXXERRORCHECKUNEXPECTSTAT(stat)      \
    }

inline HpackStatus IntegerDecode(HpRBuffer &r,
                                 const Integer2byte prefix,
                                 Uinteger5byte &i)
{
#if DEBUGFLAG == 1
    if (prefix > 8)
    {
        printf("IntegerDecode prefix(%jd) only supper less then 8.",
               (intmax_t)prefix);
    }
#endif  // DEBUGFLAG

    Uinteger5byte tmp;
    unsigned char cn = 0;
    if ((r.Current() & ((1 << prefix) - 1)) == ((1 << prefix) - 1))
    {
        i = (1 << prefix) - 1;
        do
        {
            if (!r.Next())
            {
                return HpackStatus::RBuffEndAtIntegerDecode;
            }
            tmp = (r.Current() & 0x7f);
            i = i + (tmp << (7 * cn));

            if (cn++ > (unsigned char)(sizeof(i) / sizeof(unsigned char)))
            {
                return HpackStatus::IntegerOverFlow;
            }
        } while ((r.Current() & 0x80) != 0);
    }
    else
    {
        i = (Uinteger5byte)(r.Current() & ((1 << prefix) - 1));
    }
    return HpackStatus::Success;
}

#define INTEGERDECODEERRORCHECK(stat)              \
    switch (stat)                                  \
    {                                              \
        case HpackStatus::Success:                 \
            break;                                 \
        case HpackStatus::IntegerOverFlow:         \
        case HpackStatus::RBuffEndAtIntegerDecode: \
            return (stat);                         \
        default:                                   \
            XXXERRORCHECKUNEXPECTSTAT(stat)        \
    }

class HpRBufferCache
{
    HpRBuffer &r;
    unsigned char cacheBit = 0;
    Uinteger5byte strLength, strpos = 0, huffmanCode = 0;

  public:
    static const unsigned char CacheSuccess = 1;
    static const unsigned char StopAtRBuffEnd = 2;
    static const unsigned char StopAtStringLength = 3;

    HpRBufferCache(HpRBuffer &_r, Uinteger5byte strlength)
        : r(_r), strLength(strlength)
    {
        ;
    }

    unsigned char ReadUntilBit(const HuffmanTable &hufftb,
                               size_t &switchtableindex)
    {
        Uinteger5byte tmp5byte_t;
#if DEBUGFLAG == 1
        if (hufftb.thisBit >= BitOfUinteger5Byte)
        {
            printf(
                "HpRBufferCache::ReadUntilBit thisbit(%u) >= "
                "BitOfUinteger5Byte(%u).\n",
                hufftb.thisBit,
                BitOfUinteger5Byte);
        }
#endif  // DEBUGFLAG
        while (cacheBit < hufftb.thisBit)
        {
            if (strpos++ >= strLength)
            {
                return StopAtStringLength;
            }
            if (!r.Next())
            {
                return StopAtRBuffEnd;
            }
            tmp5byte_t = ((Uinteger5byte)r.Current())
                         << (BitOfUinteger5Byte - 8 - cacheBit);
            // printf("ReadUntilBit tmp5byte_t(%llx) BitOfUinteger5Byte(%u)
            // shift(%u).\n", tmp5byte_t, BitOfUinteger5Byte,
            // (BitOfUinteger5Byte - 8 - cacheBit));
#if DEBUGFLAG == 1
            if ((huffmanCode | tmp5byte_t) != huffmanCode + tmp5byte_t)
            {
                printf(
                    "HpRBufferCache::ReadUntilBit shift(%u bit) Error "
                    "huffmanCode(0x%llx),tmp5byte_t(0x%llx).\n",
                    BitOfUinteger5Byte - 8 - cacheBit,
                    huffmanCode,
                    tmp5byte_t);
            }
#endif  // DEBUGFLAG
            cacheBit += 8;
            huffmanCode = huffmanCode | tmp5byte_t;
            // printf("ReadUntilBit strpos(%llx) tmp5byte_t(%llx)
            // huffmanCode(%llx) cacheBit(%u).\n", strpos, tmp5byte_t,
            // huffmanCode, cacheBit);
        }

        switchtableindex =
            ((size_t)(huffmanCode >> (BitOfUinteger5Byte - hufftb.thisBit))) &
            hufftb.tableAnd;
        return CacheSuccess;
    }

    void NextHuffmanCode(const unsigned char thisbit)
    {
        huffmanCode = huffmanCode << thisbit;
        cacheBit = cacheBit - thisbit;
        return;
    }

    HpackStatus CheckStopAtStringLength()
    {
        Uinteger5byte tmp = 1;
        if (cacheBit > 7)
        {
            return HpackStatus::LiteralPadLongThan7Bit;
        }
        if ((huffmanCode >> (BitOfUinteger5Byte - cacheBit)) !=
            ((cacheBit == 0) ? (Uinteger5byte)0 : (tmp << cacheBit) - 1))
        {
            return HpackStatus::LiteralPadNotEOSMsb;
        }
        return HpackStatus::Success;
    }
};

inline HpackStatus StringLiteralDecodeHuffman(HpRBuffer &r,
                                              std::string &str,
                                              Uinteger5byte strLength)
{
    size_t tableindx;
    Integer2byte sym;
    unsigned char rbcstat;
    HpRBufferCache rbc(r, strLength);
    HuffmanTable const *hufftb = nullptr;

    while (1)
    {
#if DEBUGFLAG == 1
        sym = SymbolInitial;
#endif  // DEBUGFLAG
        hufftb = &HuffmanDecodeTable[0];
        do
        {
            rbcstat = rbc.ReadUntilBit(*hufftb, tableindx);
            switch (rbcstat)
            {
                case HpRBufferCache::CacheSuccess:
                    break;
                case HpRBufferCache::StopAtRBuffEnd:
                    return HpackStatus::RBuffEndAtLiteralDecode;
                case HpRBufferCache::StopAtStringLength:
                    goto ENDATSTRLENGTH;
#if DEBUGFLAG == 1
                default:
                    printf(
                        "StringLiteralDecodeHuffman __HuffmanSwitch return "
                        "unknown status(%u).\n",
                        (unsigned int)rbcstat);
#endif  // DEBUGFLAG
            }
#if DEBUGFLAG == 1
            const HuffmanTableColumn &huffcol(hufftb->table.at(tableindx));
#else
            const HuffmanTableColumn &huffcol(hufftb->table[tableindx]);
#endif  // DEBUGFLAG
            if (huffcol.isSymbol)
            {
                sym = huffcol.symbol;
                break;
            }
            else
            {
                hufftb = &(HuffmanDecodeTable[huffcol.huffmanTable]);
            }
        } while (1);

        rbc.NextHuffmanCode(hufftb->thisBit);
        switch (sym)
        {
            case SymbolInitial:
                printf(
                    "StringLiteralDecodeHuffman __HuffmanSwitch "
                    "Destruction.\n");
                break;
            case SymbolEOS:
                return HpackStatus::LiteralContainEOS;
            default:
                str.push_back(sym);
        }
    }
ENDATSTRLENGTH:
    if (LiteralDecodeCheckLiteralPad)
    {
        return rbc.CheckStopAtStringLength();
    }
    else
    {
        return HpackStatus::Success;
    }
}

inline HpackStatus StringLiteralDecodeOctet(HpRBuffer &r,
                                            std::string &str,
                                            const Uinteger5byte strLength)
{
    if (DecodeStringLiteralOctet)
    {
        str.reserve(strLength);
        for (Uinteger5byte i = 0; i < strLength; ++i)
        {
            if (r.Next())
            {
                str.append(1, r.Current());
            }
            else
            {
                return HpackStatus::RBuffEndAtLiteralDecode;
            }
        }
        return HpackStatus::Success;
    }
    else
    {
        return HpackStatus::StringLiteralOctet;
    }
}

// str new and clean bt callee
inline HpackStatus StringLiteralDecode(HpRBuffer &r, std::string &str)
{
    bool isHuffman;
    HpackStatus stat;
    Uinteger5byte strLength;

    if ((r.Current() & HuffmanLiteralFlag) == HuffmanLiteralFlag)
    {
        isHuffman = true;
    }
    else
    {
        isHuffman = false;
    }
    stat = IntegerDecode(r, 7, strLength);
    INTEGERDECODEERRORCHECK(stat);
    if (isHuffman)
    {
        stat = StringLiteralDecodeHuffman(r, str, strLength);
    }
    else
    {
        stat = StringLiteralDecodeOctet(r, str, strLength);
    }

    return stat;
}

#define LITERALDECODEERRORCHECK(stat)              \
    switch (stat)                                  \
    {                                              \
        case HpackStatus::Success:                 \
            break;                                 \
        case HpackStatus::LiteralPadNotEOSMsb:     \
        case HpackStatus::LiteralContainEOS:       \
        case HpackStatus::LiteralPadLongThan7Bit:  \
        case HpackStatus::RBuffEndAtLiteralDecode: \
        case HpackStatus::StringLiteralOctet:      \
        case HpackStatus::IntegerOverFlow:         \
        case HpackStatus::RBuffEndAtIntegerDecode: \
            return (stat);                         \
        default:                                   \
            XXXERRORCHECKUNEXPECTSTAT(stat)        \
    }

inline HpackStatus StringLiteralEncodeHuffman(HpWBuffer &w,
                                              const std::string &str)
{
    Uinteger5byte buffer = 0;
    unsigned char wa, bitsInBuffer = 0;

    for (const unsigned char ch : str)
    {
        unsigned char nbits = HuffmanCodeTable[ch].lengthOfBit;
        Uinteger5byte msbVal = HuffmanCodeTable[ch].msbValue;

        buffer = msbVal >> bitsInBuffer | buffer;
        bitsInBuffer += nbits;

        while (bitsInBuffer >= 8)
        {
            if (!w.Append(buffer >> (BitOfUinteger5Byte - 8)))
            {
                return HpackStatus::WBuffErrLiteralEecode;
            }
            buffer = buffer << 8;
            bitsInBuffer -= 8;
        }
    }

    if (bitsInBuffer != 0)
    {
        wa = (buffer >> (BitOfUinteger5Byte - 8)) |
             ((1 << (8 - bitsInBuffer)) - 1);
        if (!w.Append(wa))
        {
            return HpackStatus::WBuffErrLiteralEecode;
        }
    }
    return HpackStatus::Success;
}

template <StringLiteralEncodeWay SLEF>
inline HpackStatus StringLiteralEncode(HpWBuffer &w, const std::string &str)
{
    HpackStatus stat;
    Uinteger5byte encodelen;
    Uinteger5byte huffstrlen = 0;
    unsigned char encodetypebit;

    if ((SLEF == StringLiteralEncodeWay::EShortest) ||
        (SLEF == StringLiteralEncodeWay::EHuffman))
    {
        for (const unsigned char c : str)
        {
            huffstrlen += HuffmanCodeTable[c].lengthOfBit;
        }
        huffstrlen = (huffstrlen / 8) + ((huffstrlen % 8) == 0 ? 0 : 1);
    }

    if (SLEF == StringLiteralEncodeWay::EShortest)
    {
        if (huffstrlen < str.size())
        {
            encodelen = huffstrlen;
            encodetypebit = HuffmanLiteralFlag;
        }
        else
        {
            encodelen = str.size();
            encodetypebit = 0;
        }
    }
    else if (SLEF == StringLiteralEncodeWay::EHuffman)
    {
        encodelen = huffstrlen;
        encodetypebit = HuffmanLiteralFlag;
    }
    else if (SLEF == StringLiteralEncodeWay::EOctet)
    {
        encodelen = str.size();
        encodetypebit = 0;
    }

    w.OrCurrent(encodetypebit);
    stat = IntegerEncode(w, 7, encodelen);
    INTEGERENCODEERRORCHECK(stat);
    if (encodetypebit == HuffmanLiteralFlag)
    {
        stat = StringLiteralEncodeHuffman(w, str);
    }
    else
    {
        for (const unsigned char c : str)
        {
            if (!w.Append(c))
            {
                return HpackStatus::WBuffErrLiteralEecode;
            }
        }
    }
    return HpackStatus::Success;
}

#define LITERALENCODEERRORCHECK(stat)            \
    switch (stat)                                \
    {                                            \
        case HpackStatus::Success:               \
            break;                               \
        case HpackStatus::WBuffErrLiteralEecode: \
            return (stat);                       \
        default:                                 \
            XXXERRORCHECKUNEXPECTSTAT(stat)      \
    }

class IndexTable
{
  private:
    size_t dynamicTableSize;
    // Must not zero
    size_t dynamicTableMaxSize;
    std::deque<KeyValPair> dynamicTable;

    inline size_t CalculatSize(const KeyValPair &kvp)
    {
        // RFC 7541 4.1. Calculating Table Size
        return kvp.first.size() + kvp.second.size() + 32u;
    }

    inline size_t DynamicTableEvict()
    {
        size_t evictcn = 0;
        while (dynamicTableSize > dynamicTableMaxSize)
        {
#if DEBUGFLAG == 1
            if (dynamicTable.size() == 0)
            {
                printf(
                    "IndexTable::DynamicTableEvict dynamicTable and "
                    "dynamicTableSize out of sync dynamicTable::size %zu "
                    "dynamicTableSize %zu.\n",
                    dynamicTable.size(),
                    dynamicTableSize);
            }
#endif  // DEBUGFLAG
            evictcn += 1;
            dynamicTableSize -= CalculatSize(dynamicTable.back());
            dynamicTable.pop_back();
        }
        return evictcn;
    }

  public:
    IndexTable(const size_t dynamictablemaxsize)
        : dynamicTableSize(0),
          dynamicTableMaxSize(dynamictablemaxsize),
          dynamicTable()
    {
        ;
    }

    // Index start at 1
    inline HpackStatus at(size_t i, KeyValPair &kvp)
    {
        size_t dynpos;
        if (i == 0)
        {
            return HpackStatus::TableIndexZero;
        }
        if (i < StaticTableSize)
        {
            kvp = StaticTable[i];
            return HpackStatus::Success;
        }
        dynpos = i - StaticTableSize;
        if (dynpos < dynamicTable.size())
        {
            kvp = dynamicTable[dynpos];
            return HpackStatus::Success;
        }
#if DEBUGFLAG == 1
        printf("IndexTable::at(%zu) TableIndeNotExist.\n", i);
#endif  // DEBUGFLAG
        return HpackStatus::TableIndeNotExist;
    }

    // StaticTable has priority in search. If two tables have the same key,
    // StaticTable will be used. StaticTableSize is the real length of
    // StaticTable
    inline size_t find(const KeyValPair &kvp, bool &onlymatchkey)
    {
        size_t indx = 0, i = 0;
        std::unordered_map<
            std::string,
            const std::vector<StaticTableMapElement> >::const_iterator iter;

        onlymatchkey = false;
        if ((iter = StaticTableMap.find(kvp.first)) != StaticTableMap.end())
        {
            indx = iter->second[0].index;
            for (const StaticTableMapElement &ele : iter->second)
            {
                if (ele.value == kvp.second)
                {
                    return ele.index;
                }
            }
        }

        for (const KeyValPair &dynkvp : dynamicTable)
        {
            if (dynkvp.first == kvp.first)
            {
                if (dynkvp.second == kvp.second)
                {
                    return StaticTableSize + i;
                }
                if (indx == 0)
                {
                    indx = StaticTableSize + i;
                }
            }
            ++i;
        }

        onlymatchkey = true;
        return indx;
    }

    inline void DynamicTableInsert(const KeyValPair &kvp, size_t &evictcn)
    {
        dynamicTableSize += CalculatSize(kvp);
        dynamicTable.push_front(kvp);
        evictcn = DynamicTableEvict();
        return;
    }

    inline void DynamicTableSizeUpdate(const size_t newsize, size_t &evictcn)
    {
        dynamicTableMaxSize = newsize;
        evictcn = DynamicTableEvict();
        return;
    }

    inline size_t DynamicTableSize()
    {
        return dynamicTableSize;
    }

    inline size_t DynamicTableMaxSize()
    {
        return dynamicTableMaxSize;
    }
#if DEBUGFLAG == 1
    std::deque<KeyValPair> DynamicTable()
    {
        return dynamicTable;
    }
#endif  // DEBUGFLAG
};

#define INDEXTABLEATEERRORCHECK(stat)        \
    switch (stat)                            \
    {                                        \
        case HpackStatus::Success:           \
            break;                           \
        case HpackStatus::TableIndexZero:    \
        case HpackStatus::TableIndeNotExist: \
            return stat;                     \
        default:                             \
            XXXERRORCHECKUNEXPECTSTAT(stat)  \
    }

#if DEBUGFLAG == 1
void IndexTablePrint(IndexTable &inxt)
{
    for (KeyValPair kvp : inxt.DynamicTable())
    {
        KeyValPairPrint(kvp);
    }
}
#endif  // DEBUGFLAG

class _Hpack
{
  public:
    const size_t *SETTINGS_HEADER_TABLE_SIZE;
    IndexTable indexTable;
#if DEBUGFLAG == 1
    size_t errorLine = 0;
#endif  // DEBUGFLAG
    template <bool HFIF>
    inline HpackStatus _DecoderLoop(HpRBuffer &stream,
                                    KeyValPair &kvp,
                                    HeaderFieldInfo &hfdi)
    {
        bool readkey;
        HpackStatus stat;
        Uinteger5byte indx;

        indx = 0;
        readkey = false;
        if ((stream.Current() & IndexedHeaderField) == IndexedHeaderField)
        {
            if (HFIF)
            {
                hfdi.representation =
                    HeaderFieldRepresentation::RIndexedHeaderField;
            }
            stat = IntegerDecode(stream, 7, indx);
            INTEGERDECODEERRORCHECK(stat);
            stat = indexTable.at(indx, kvp);
            INDEXTABLEATEERRORCHECK(stat);
            return HpackStatus::Success;
        }
        else if ((stream.Current() &
                  LiteralHeaderFieldWithIncrementalIndexing) ==
                 LiteralHeaderFieldWithIncrementalIndexing)
        {
            hfdi.representation = HeaderFieldRepresentation::
                RLiteralHeaderFieldWithIncrementalIndexing;
            stat = IntegerDecode(stream, 6, indx);
            INTEGERDECODEERRORCHECK(stat);
        }
        else if ((stream.Current() & DynamicTableSizeUpdate) ==
                 DynamicTableSizeUpdate)
        {
            if (HFIF || !HpackDecoderTableUpdateHeaderBlank)
            {
                hfdi.representation =
                    HeaderFieldRepresentation::RDynamicTableSizeUpdate;
            }
            stat = IntegerDecode(stream, 5, indx);
            INTEGERDECODEERRORCHECK(stat);
            if (indx > *SETTINGS_HEADER_TABLE_SIZE)
            {
#if DEBUGFLAG == 1
                printf(
                    "_Hpack::_DecoderLoop TableSizeOverflow "
                    "SETTINGS_HEADER_TABLE_SIZE(%ju) indx(%ju).\n",
                    (uintmax_t)(*SETTINGS_HEADER_TABLE_SIZE),
                    (uintmax_t)indx);
#endif  // DEBUGFLAG
                return HpackStatus::TableSizeOverflow;
            }
            indexTable.DynamicTableSizeUpdate(indx, hfdi.evictCounter);
            return HpackStatus::Success;
        }
        else if ((stream.Current() & LiteralHeaderFieldNeverIndexed) ==
                 LiteralHeaderFieldNeverIndexed)
        {
            if (HFIF)
            {
                hfdi.representation =
                    HeaderFieldRepresentation::RLiteralHeaderFieldNeverIndexed;
            }
            stat = IntegerDecode(stream, 4, indx);
            INTEGERDECODEERRORCHECK(stat);
        }
        else
        {
            if (HFIF)
            {
                hfdi.representation = HeaderFieldRepresentation::
                    RLiteralHeaderFieldWithoutIndexing;
            }
            stat = IntegerDecode(stream, 4, indx);
            INTEGERDECODEERRORCHECK(stat);
        }
        if (indx == 0)
        {
            readkey = true;
        }
        else
        {
            stat = indexTable.at(indx, kvp);
            INDEXTABLEATEERRORCHECK(stat);
            kvp.second.clear();
        }
        if (readkey)
        {
            if (!stream.Next())
            {
                return HpackStatus::RBuffEndAtHeaderField;
            }
            stat = StringLiteralDecode(stream, kvp.first);
            LITERALDECODEERRORCHECK(stat);
        }
        if (!stream.Next())
        {
            return HpackStatus::RBuffEndAtHeaderField;
        }
        stat = StringLiteralDecode(stream, kvp.second);
        LITERALDECODEERRORCHECK(stat);
        if (hfdi.representation ==
            HeaderFieldRepresentation::
                RLiteralHeaderFieldWithIncrementalIndexing)
        {
            if (!HFIF)
            {
                hfdi.representation = HeaderFieldRepresentation::RInitial;
            }
            indexTable.DynamicTableInsert(kvp, hfdi.evictCounter);
        }
        return HpackStatus::Success;
    }

    template <bool HFIF>
    inline HpackStatus _Decoder(HpRBuffer &stream,
                                std::vector<KeyValPair> &header,
                                std::vector<HeaderFieldInfo> *headerfieldinfo)
    {
        KeyValPair kvp;
        HpackStatus stat;
        HeaderFieldInfo hfdi;
#if DEBUGFLAG == 1
        size_t linecounter = 0;
#endif  // DEBUGFLAG
        if (HpackDecoderCheckParameter)
        {
            if (stream.Size() == 0)
            {
                return HpackStatus::RBuffZeroLength;
            }
        }

        do
        {
            if ((stat = _DecoderLoop<HFIF>(stream, kvp, hfdi)) !=
                HpackStatus::Success)
            {
#if DEBUGFLAG == 1
                errorLine = linecounter;
#endif  // DEBUGFLAG
                return stat;
            }
            if (!HpackDecoderTableUpdateHeaderBlank && !HFIF)
            {
                if (hfdi.representation ==
                    HeaderFieldRepresentation::RDynamicTableSizeUpdate)
                {
                    hfdi.representation = HeaderFieldRepresentation::RInitial;
                }
                else
                {
                    header.push_back(kvp);
                }
            }
            else
            {
                header.push_back(kvp);
            }

            if (HFIF)
            {
                headerfieldinfo->push_back(hfdi);
                hfdi.evictCounter = 0;
            }
            kvp.first.clear();
            kvp.second.clear();
#if DEBUGFLAG == 1
            linecounter++;
#endif  // DEBUGFLAG
        } while (stream.Next());
        return HpackStatus::Success;
    }

    inline HpackStatus _EncoderLoopIndexed(HpWBuffer &stream,
                                           const size_t index)
    {
        // Indexed Header Field
        HpackStatus stat;
        stream.Append(IndexedHeaderField);
        stat = IntegerEncode(stream, 7, index);
        INTEGERENCODEERRORCHECK(stat);
        return HpackStatus::Success;
    }

    template <StringLiteralEncodeWay SLEF>
    inline HpackStatus _EncoderLoopLiteralHeader(HpWBuffer &stream,
                                                 const KeyValPair &kvp,
                                                 const size_t index,
                                                 HeaderFieldRepresentation hfrp)
    {
        HpackStatus stat;
        unsigned char fieldrepr, indexbit;
        switch (hfrp)
        {
            case HeaderFieldRepresentation::
                RLiteralHeaderFieldWithIncrementalIndexing:
                indexbit = 6;
                fieldrepr = LiteralHeaderFieldWithIncrementalIndexing;
                break;
            case HeaderFieldRepresentation::RLiteralHeaderFieldWithoutIndexing:
                indexbit = 4;
                fieldrepr = LiteralHeaderFieldWithoutIndexing;
                break;
            case HeaderFieldRepresentation::RLiteralHeaderFieldNeverIndexed:
                indexbit = 4;
                fieldrepr = LiteralHeaderFieldNeverIndexed;
                break;
            default:
                return HpackStatus::HpackEncodeFieldRepresent;
        }
        if (!stream.Append(fieldrepr))
        {
            return HpackStatus::WBuffErrHeaderField;
        }
        if (index == 0)
        {
            if (!stream.Append(0))
            {
                return HpackStatus::WBuffErrHeaderField;
            }
            stat = StringLiteralEncode<SLEF>(stream, kvp.first);
            LITERALENCODEERRORCHECK(stat);
        }
        else
        {
            stat = IntegerEncode(stream, indexbit, (Uinteger5byte)index);
            INTEGERENCODEERRORCHECK(stat);
        }
        if (!stream.Append(0))
        {
            return HpackStatus::WBuffErrHeaderField;
        }
        stat = StringLiteralEncode<SLEF>(stream, kvp.second);
        LITERALENCODEERRORCHECK(stat);
        return HpackStatus::Success;
    }

    template <StringLiteralEncodeWay SLEF>
    inline HpackStatus _EncoderLoopAlwaysIndex(HpWBuffer &stream,
                                               const KeyValPair &kvp,
                                               const size_t index,
                                               const bool onlymatchkey,
                                               HeaderFieldInfo &hfdi)
    {
        HpackStatus stat = HpackStatus::Success;
        // printf("_EncoderLoopAlwaysIndex index %lu onlymatchkey %d.\n",index,
        // onlymatchkey);
        if (index != 0 && !onlymatchkey)
        {
            stat = _EncoderLoopIndexed(stream, index);
            hfdi.representation =
                HeaderFieldRepresentation::RIndexedHeaderField;
        }
        else
        {
            stat = _EncoderLoopLiteralHeader<SLEF>(
                stream,
                kvp,
                index,
                HeaderFieldRepresentation::
                    RLiteralHeaderFieldWithIncrementalIndexing);
            indexTable.DynamicTableInsert(kvp, hfdi.evictCounter);
            hfdi.representation = HeaderFieldRepresentation::
                RLiteralHeaderFieldWithIncrementalIndexing;
        }
        return stat;
    }

    template <bool DOEXCLIDE, StringLiteralEncodeWay SLEF>
    inline HpackStatus _Encoder(HpWBuffer &stream,
                                const std::vector<KeyValPair> &header,
                                const std::vector<std::string> *exclude)
    {
        size_t indx;
        HpackStatus stat;
        bool onlymatchkey, isexclude;
        HeaderFieldInfo hfdi;

        for (const KeyValPair &kvp : header)
        {
            if (DOEXCLIDE)
            {
                isexclude = false;
                for (const std::string &excludekey : *exclude)
                {
                    if (kvp.first == excludekey)
                    {
                        isexclude = true;
                        break;
                    }
                }
            }

            indx = indexTable.find(kvp, onlymatchkey);
            if (DOEXCLIDE)
            {
                if (isexclude)
                {
                    stat = _EncoderLoopLiteralHeader<SLEF>(
                        stream,
                        kvp,
                        indx,
                        HeaderFieldRepresentation::
                            RLiteralHeaderFieldNeverIndexed);
                }
                else
                {
                    stat = _EncoderLoopAlwaysIndex<SLEF>(
                        stream, kvp, indx, onlymatchkey, hfdi);
                }
            }
            else
            {
                stat = _EncoderLoopAlwaysIndex<SLEF>(
                    stream, kvp, indx, onlymatchkey, hfdi);
            }

            if (stat != HpackStatus::Success)
            {
                return stat;
            }
        }
        return HpackStatus::Success;
    }

    template <StringLiteralEncodeWay SLEF>
    inline HpackStatus _EncoderHeaderFieldInfo(
        HpWBuffer &stream,
        const std::vector<KeyValPair> &header,
        std::vector<HeaderFieldInfo> &hfdilst)
    {
        size_t indx;
        HpackStatus stat;
        bool onlymatchkey;

        std::vector<KeyValPair>::const_iterator kvpiter = header.cbegin();
        std::vector<HeaderFieldInfo>::iterator hfiter = hfdilst.begin();
        if (header.size() != hfdilst.size())
        {
            return HpackStatus::HfdiListLength;
        }

        while (kvpiter != header.end())
        {
            indx = indexTable.find(*kvpiter, onlymatchkey);
            switch (hfiter->representation)
            {
                case HeaderFieldRepresentation::RLiteralHeaderFieldAlwaysIndex:
                    stat = _EncoderLoopAlwaysIndex<SLEF>(
                        stream, *kvpiter, indx, onlymatchkey, (*hfiter));
                    break;
                case HeaderFieldRepresentation::
                    RLiteralHeaderFieldWithIncrementalIndexing:
                    stat = _EncoderLoopLiteralHeader<SLEF>(
                        stream,
                        *kvpiter,
                        indx,
                        HeaderFieldRepresentation::
                            RLiteralHeaderFieldWithIncrementalIndexing);
                    indexTable.DynamicTableInsert(*kvpiter,
                                                  hfiter->evictCounter);
                    break;
                case HeaderFieldRepresentation::
                    RLiteralHeaderFieldWithoutIndexing:
                case HeaderFieldRepresentation::RLiteralHeaderFieldNeverIndexed:
                    stat = _EncoderLoopLiteralHeader<SLEF>(
                        stream, *kvpiter, indx, hfiter->representation);
                    break;
                default:
                    stat = HpackStatus::HpackEncodeFieldRepresent;
                    break;
            }
            if (stat != HpackStatus::Success)
            {
                return stat;
            }
            hfiter++;
            kvpiter++;
        }
        return HpackStatus::Success;
    }

    template <bool DOEXCLIDE>
    inline HpackStatus _Encoder(HpWBuffer &stream,
                                const std::vector<KeyValPair> &header,
                                const std::vector<std::string> *exclude,
                                const StringLiteralEncodeWay SLEW)
    {
        switch (SLEW)
        {
            case StringLiteralEncodeWay::EOctet:
                return _Encoder<DOEXCLIDE, StringLiteralEncodeWay::EOctet>(
                    stream, header, exclude);
            case StringLiteralEncodeWay::EHuffman:
                return _Encoder<DOEXCLIDE, StringLiteralEncodeWay::EHuffman>(
                    stream, header, exclude);
            case StringLiteralEncodeWay::EShortest:
                return _Encoder<DOEXCLIDE, StringLiteralEncodeWay::EShortest>(
                    stream, header, exclude);
            default:
                return HpackStatus::LiteralEncodeWay;
        }
    }

    inline HpackStatus _EncoderDynamicTableSizeUpdate(HpWBuffer &stream,
                                                      const size_t newSize,
                                                      HeaderFieldInfo &hfdi)
    {
        HpackStatus stat;
        if (!stream.Append(DynamicTableSizeUpdate))
        {
            return HpackStatus::WBuffErrHeaderField;
        }
        stat = IntegerEncode(stream, 5, (Uinteger5byte)newSize);
        INTEGERDECODEERRORCHECK(stat);
        indexTable.DynamicTableSizeUpdate(newSize, hfdi.evictCounter);
        hfdi.representation =
            HeaderFieldRepresentation::RDynamicTableSizeUpdate;
        return HpackStatus::Success;
    }

    _Hpack()
        : SETTINGS_HEADER_TABLE_SIZE(&SETTINGS_HEADER_TABLE_SIZE_DEFAULT),
          indexTable(*SETTINGS_HEADER_TABLE_SIZE)
    {
        ;
    }

    _Hpack(size_t *settingheadertablesize)
        : SETTINGS_HEADER_TABLE_SIZE(settingheadertablesize),
          indexTable(*SETTINGS_HEADER_TABLE_SIZE)
    {
        // printf("Hpack::Constructor with SETTINGS_HEADER_TABLE_SIZE(%ld).\n",
        // *SETTINGS_HEADER_TABLE_SIZE);
    }
};

Hpack::Hpack() : _hpack(new _Hpack())
{
    ;
}

Hpack::Hpack(size_t *settingheadertablesize)
    : _hpack(new _Hpack(settingheadertablesize))
{
    ;
}

HpackStatus Hpack::Encoder(HpWBuffer &stream,
                           const std::vector<KeyValPair> &header) const
{
    return _hpack->_Encoder<false, StringLiteralEncodeWay::EShortest>(stream,
                                                                      header,
                                                                      nullptr);
}

HpackStatus Hpack::Encoder(HpWBuffer &stream,
                           const std::vector<KeyValPair> &header,
                           const std::vector<std::string> &exclude) const
{
    return _hpack->_Encoder<true, StringLiteralEncodeWay::EShortest>(stream,
                                                                     header,
                                                                     &exclude);
}

HpackStatus Hpack::Encoder(HpWBuffer &stream,
                           const std::vector<KeyValPair> &header,
                           std::vector<HeaderFieldInfo> &hfdilst) const
{
    return _hpack->_EncoderHeaderFieldInfo<StringLiteralEncodeWay::EShortest>(
        stream, header, hfdilst);
}

HpackStatus Hpack::Decoder(HpRBuffer &stream,
                           std::vector<KeyValPair> &header) const
{
    return _hpack->_Decoder<false>(stream, header, nullptr);
}

HpackStatus Hpack::Decoder(HpRBuffer &stream,
                           std::vector<KeyValPair> &header,
                           std::vector<HeaderFieldInfo> &hfdilst) const
{
    return _hpack->_Decoder<true>(stream, header, &hfdilst);
}

HpackStatus Hpack::Encoder(std::unique_ptr<HpWBuffer> stream,
                           const std::vector<KeyValPair> &header) const
{
    return _hpack->_Encoder<false, StringLiteralEncodeWay::EShortest>(*stream,
                                                                      header,
                                                                      nullptr);
}

HpackStatus Hpack::Encoder(std::unique_ptr<HpWBuffer> stream,
                           const std::vector<KeyValPair> &header,
                           const std::vector<std::string> &exclude) const
{
    return _hpack->_Encoder<true, StringLiteralEncodeWay::EShortest>(*stream,
                                                                     header,
                                                                     &exclude);
}

HpackStatus Hpack::Encoder(std::unique_ptr<HpWBuffer> stream,
                           const std::vector<KeyValPair> &header,
                           std::vector<HeaderFieldInfo> &hfdilst) const
{
    return _hpack->_EncoderHeaderFieldInfo<StringLiteralEncodeWay::EShortest>(
        *stream, header, hfdilst);
}

HpackStatus Hpack::Decoder(std::unique_ptr<HpRBuffer> stream,
                           std::vector<KeyValPair> &header) const
{
    return _hpack->_Decoder<false>(*stream, header, nullptr);
}

HpackStatus Hpack::Decoder(std::unique_ptr<HpRBuffer> stream,
                           std::vector<KeyValPair> &header,
                           std::vector<HeaderFieldInfo> &hfdilst) const
{
    return _hpack->_Decoder<true>(*stream, header, &hfdilst);
}

HpackStatus Hpack::Encoder(HpWBuffer &stream,
                           const std::vector<KeyValPair> &header,
                           const StringLiteralEncodeWay SLEW) const
{
    return _hpack->_Encoder<false>(stream, header, nullptr, SLEW);
}

HpackStatus Hpack::Encoder(HpWBuffer &stream,
                           const std::vector<KeyValPair> &header,
                           const std::vector<std::string> &exclude,
                           const StringLiteralEncodeWay SLEW) const
{
    return _hpack->_Encoder<true>(stream, header, &exclude, SLEW);
}

HpackStatus Hpack::Encoder(HpWBuffer &stream,
                           const std::vector<KeyValPair> &header,
                           std::vector<HeaderFieldInfo> &hfdilst,
                           const StringLiteralEncodeWay SLEW) const
{
    switch (SLEW)
    {
        case StringLiteralEncodeWay::EOctet:
            return _hpack
                ->_EncoderHeaderFieldInfo<StringLiteralEncodeWay::EOctet>(
                    stream, header, hfdilst);
        case StringLiteralEncodeWay::EHuffman:
            return _hpack
                ->_EncoderHeaderFieldInfo<StringLiteralEncodeWay::EHuffman>(
                    stream, header, hfdilst);
        case StringLiteralEncodeWay::EShortest:
            return _hpack
                ->_EncoderHeaderFieldInfo<StringLiteralEncodeWay::EShortest>(
                    stream, header, hfdilst);
        default:
            return HpackStatus::LiteralEncodeWay;
    }
}

HpackStatus Hpack::EncoderDynamicTableSizeUpdate(HpWBuffer &stream,
                                                 const size_t newSize,
                                                 HeaderFieldInfo &hfdi)
{
    return _hpack->_EncoderDynamicTableSizeUpdate(stream, newSize, hfdi);
}

std::unique_ptr<HpRBuffer> Hpack::MakeHpRBuffer(
    const std::vector<unsigned char> &ss)
{
    return std::unique_ptr<HpRBuffer>(new VecHpRBuffer(ss));
}

std::unique_ptr<HpRBuffer> Hpack::MakeHpRBuffer(const unsigned char *start,
                                                size_t sizebyte)
{
    return std::unique_ptr<HpRBuffer>(new ArrHpRBuffer(start, sizebyte));
}

std::unique_ptr<HpWBuffer> Hpack::MakeHpWBuffer(std::vector<unsigned char> &ss)
{
    return std::unique_ptr<HpWBuffer>(new VecHpWBuffer(ss));
}

std::unique_ptr<HpWBuffer> Hpack::MakeHpWBuffer(std::vector<unsigned char> &ss,
                                                size_t maxsize)
{
    return std::unique_ptr<HpWBuffer>(new VecHpWBuffer(ss, maxsize));
}

const std::string Hpack::HpackStatus2String(HpackStatus stat)
{
    return std::string(HpackStatus2CString(stat));
}

size_t Hpack::DynamicTableSize() const
{
    return (_hpack->indexTable).DynamicTableSize();
}

size_t Hpack::DynamicTableMaxSize() const
{
    return (_hpack->indexTable).DynamicTableMaxSize();
}

#if DEBUGFLAG == 1
IndexTable &Hpack::_IndexTable()
{
    return _hpack->indexTable;
}
#endif  // DEBUGFLAG
Hpack::~Hpack()
{
    delete _hpack;
}
