#pragma once
#include <string>
#include <vector>
#include <memory>

#ifndef DEBUGFLAG
#define DEBUGFLAG 0
#endif  // DEBUGFLAG

typedef std::pair<std::string, std::string> KeyValPair;

enum HpackStatus
{
    Success,
    IntegerOverFlow,
    TableIndexZero,
    TableIndeNotExist,
    StringLiteralOctet,
    LiteralPadNotEOSMsb,
    LiteralContainEOS,
    LiteralPadLongThan7Bit,
    RBuffEndAtIntegerDecode,
    RBuffEndAtLiteralDecode,
    RBuffEndAtHeaderField,
    RBuffZeroLength,
    WBuffErrIntegerEecode,
    WBuffErrLiteralEecode,
    WBuffErrHeaderField,
    HpackEncodeFieldRepresent,
    DecodeSwitchEnd,
    HfdiListLength,
    LiteralEncodeWay,
    TableSizeOverflow,
};

enum HeaderFieldRepresentation
{
    RInitial,
    RIndexedHeaderField,
    RLiteralHeaderFieldWithIncrementalIndexing,
    RDynamicTableSizeUpdate,
    RLiteralHeaderFieldNeverIndexed,
    RLiteralHeaderFieldWithoutIndexing,
    RLiteralHeaderFieldAlwaysIndex
};

enum StringLiteralEncodeWay
{
    EOctet,
    EHuffman,
    EShortest
};

class HeaderFieldInfo
{
  public:
    size_t evictCounter;
    HeaderFieldRepresentation representation;

    HeaderFieldInfo()
        : evictCounter(0), representation(HeaderFieldRepresentation::RInitial)
    {
        ;
    }

    bool operator==(const HeaderFieldInfo &rhs) const noexcept;
    bool operator!=(const HeaderFieldInfo &rhs) const noexcept;
};

class HpRBuffer
{
    // 0 length not allowed
  public:
    virtual ~HpRBuffer() = default;
    virtual bool Next() = 0;
    virtual size_t Size() = 0;
    virtual unsigned char Current() = 0;
};

class HpWBuffer
{
  public:
    virtual ~HpWBuffer() = default;
    virtual size_t Size() = 0;
    virtual bool Append(unsigned char i) = 0;
    virtual unsigned char OrCurrent(unsigned char i) = 0;
};
class IndexTable;
class _Hpack;

class Hpack
{
    _Hpack *_hpack;

  public:
    Hpack();
    Hpack(size_t *settingheadertablesize);
    HpackStatus Encoder(HpWBuffer &stream,
                        const std::vector<KeyValPair> &header) const;
    HpackStatus Encoder(HpWBuffer &stream,
                        const std::vector<KeyValPair> &header,
                        const std::vector<std::string> &exclude) const;
    HpackStatus Encoder(HpWBuffer &stream,
                        const std::vector<KeyValPair> &header,
                        std::vector<HeaderFieldInfo> &hfdilst) const;
    HpackStatus Decoder(HpRBuffer &stream,
                        std::vector<KeyValPair> &header) const;
    HpackStatus Decoder(HpRBuffer &stream,
                        std::vector<KeyValPair> &header,
                        std::vector<HeaderFieldInfo> &hfdilst) const;
    HpackStatus Encoder(std::unique_ptr<HpWBuffer> stream,
                        const std::vector<KeyValPair> &header) const;
    HpackStatus Encoder(std::unique_ptr<HpWBuffer> stream,
                        const std::vector<KeyValPair> &header,
                        const std::vector<std::string> &exclude) const;
    HpackStatus Encoder(std::unique_ptr<HpWBuffer> stream,
                        const std::vector<KeyValPair> &header,
                        std::vector<HeaderFieldInfo> &hfdilst) const;
    HpackStatus Decoder(std::unique_ptr<HpRBuffer> stream,
                        std::vector<KeyValPair> &header) const;
    HpackStatus Decoder(std::unique_ptr<HpRBuffer> stream,
                        std::vector<KeyValPair> &header,
                        std::vector<HeaderFieldInfo> &hfdilst) const;
    HpackStatus Encoder(HpWBuffer &stream,
                        const std::vector<KeyValPair> &header,
                        const StringLiteralEncodeWay SLEW) const;
    HpackStatus Encoder(HpWBuffer &stream,
                        const std::vector<KeyValPair> &header,
                        const std::vector<std::string> &exclude,
                        const StringLiteralEncodeWay SLEW) const;
    HpackStatus Encoder(HpWBuffer &stream,
                        const std::vector<KeyValPair> &header,
                        std::vector<HeaderFieldInfo> &hfdilst,
                        const StringLiteralEncodeWay SLEW) const;
    HpackStatus EncoderDynamicTableSizeUpdate(HpWBuffer &stream,
                                              const size_t newSize,
                                              HeaderFieldInfo &hfdi);

    static std::unique_ptr<HpWBuffer> MakeHpWBuffer(
        std::vector<unsigned char> &ss);
    static std::unique_ptr<HpWBuffer> MakeHpWBuffer(
        std::vector<unsigned char> &ss,
        size_t maxsize);
    static std::unique_ptr<HpRBuffer> MakeHpRBuffer(
        const std::vector<unsigned char> &ss);
    static std::unique_ptr<HpRBuffer> MakeHpRBuffer(const unsigned char *start,
                                                    size_t sizebyte);
    static const std::string HpackStatus2String(HpackStatus stat);
    size_t DynamicTableSize() const;
    size_t DynamicTableMaxSize() const;
    ~Hpack();
#if DEBUGFLAG == 1
    IndexTable &_IndexTable();
#endif  // DEBUGFLAG
};
