#include <trantor/utils/MsgBuffer.h>
#include <drogon/drogon_test.h>
#include <string>
#include <iostream>

using namespace trantor;
DROGON_TEST(MsgBufferTest)
{
    SUBSECTION(readableTest)
    {
        MsgBuffer buffer;

        CHECK(buffer.readableBytes() == 0);
        buffer.append(std::string(128, 'a'));
        CHECK(buffer.readableBytes() == 128);
        buffer.retrieve(100);
        CHECK(buffer.readableBytes() == 28);
        CHECK(buffer.peekInt8() == 'a');
        buffer.retrieveAll();
        CHECK(buffer.readableBytes() == 0);
    }

    SUBSECTION(writableTest)
    {
        MsgBuffer buffer(100);

        CHECK(buffer.writableBytes() == 100);
        buffer.append("abcde");
        CHECK(buffer.writableBytes() == 95);
        buffer.append(std::string(100, 'x'));
        CHECK(buffer.writableBytes() == 111);
        buffer.retrieve(100);
        CHECK(buffer.writableBytes() == 111);
        buffer.append(std::string(112, 'c'));
        CHECK(buffer.writableBytes() == 99);
        buffer.retrieveAll();
        CHECK(buffer.writableBytes() == 216);
    }

    SUBSECTION(addInFrontTest)
    {
        MsgBuffer buffer(100);

        CHECK(buffer.writableBytes() == 100);
        buffer.addInFrontInt8('a');
        CHECK(buffer.writableBytes() == 100);
        buffer.addInFrontInt64(123);
        CHECK(buffer.writableBytes() == 92);
        buffer.addInFrontInt64(100);
        CHECK(buffer.writableBytes() == 84);
        buffer.addInFrontInt8(1);
        CHECK(buffer.writableBytes() == 84);
    }

    SUBSECTION(MoveAssignmentOperator)
    {
        MsgBuffer buf(100);
        const char *bufptr = buf.peek();
        size_t writable = buf.writableBytes();
        MsgBuffer buffnew(1000);
        buffnew = std::move(buf);
        CHECK(bufptr == buffnew.peek());
        CHECK(writable == buffnew.writableBytes());
    }
}
