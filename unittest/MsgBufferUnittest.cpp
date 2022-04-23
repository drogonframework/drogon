#include <trantor/utils/MsgBuffer.h>
#include <gtest/gtest.h>
#include <string>
#include <iostream>
using namespace trantor;
TEST(MsgBufferTest, readableTest)
{
    MsgBuffer buffer;

    EXPECT_EQ(0, buffer.readableBytes());
    buffer.append(std::string(128, 'a'));
    EXPECT_EQ(128, buffer.readableBytes());
    buffer.retrieve(100);
    EXPECT_EQ(28, buffer.readableBytes());
    EXPECT_EQ('a', buffer.peekInt8());
    buffer.retrieveAll();
    EXPECT_EQ(0, buffer.readableBytes());
}
TEST(MsgBufferTest, writableTest)
{
    MsgBuffer buffer(100);

    EXPECT_EQ(100, buffer.writableBytes());
    buffer.append("abcde");
    EXPECT_EQ(95, buffer.writableBytes());
    buffer.append(std::string(100, 'x'));
    EXPECT_EQ(111, buffer.writableBytes());
    buffer.retrieve(100);
    EXPECT_EQ(111, buffer.writableBytes());
    buffer.append(std::string(112, 'c'));
    EXPECT_EQ(99, buffer.writableBytes());
    buffer.retrieveAll();
    EXPECT_EQ(216, buffer.writableBytes());
}

TEST(MsgBufferTest, addInFrontTest)
{
    MsgBuffer buffer(100);

    EXPECT_EQ(100, buffer.writableBytes());
    buffer.addInFrontInt8('a');
    EXPECT_EQ(100, buffer.writableBytes());
    buffer.addInFrontInt64(123);
    EXPECT_EQ(92, buffer.writableBytes());
    buffer.addInFrontInt64(100);
    EXPECT_EQ(84, buffer.writableBytes());
    buffer.addInFrontInt8(1);
    EXPECT_EQ(84, buffer.writableBytes());
}

TEST(MsgBuffer, MoveContrustor)
{
    MsgBuffer buf1(100);
    const char *bufptr1 = buf1.peek();
    MsgBuffer buffnew1 = std::move(buf1);
    EXPECT_EQ(bufptr1, buffnew1.peek());

    MsgBuffer buf2(100);
    const char *bufptr2 = buf2.peek();
    MsgBuffer buffnew2(std::move(buf2));
    EXPECT_EQ(bufptr2, buffnew2.peek());
}

TEST(Msgbuffer, MoveAssignmentOperator)
{
    MsgBuffer buf(100);
    const char *bufptr = buf.peek();
    size_t writable = buf.writableBytes();
    MsgBuffer buffnew(1000);
    buffnew = std::move(buf);
    EXPECT_EQ(bufptr, buffnew.peek());
    EXPECT_EQ(writable, buffnew.writableBytes());
}
int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}