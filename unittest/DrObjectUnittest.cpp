#include <drogon/DrObject.h>
#include <gtest/gtest.h>
#include <string>
#include <iostream>

using namespace drogon;
class TestA : public DrObject<TestA>
{
};
namespace test
{
class TestB : public DrObject<TestB>
{
};
}  // namespace test
TEST(DrObjectTest, creationTest)
{
    using PtrType = std::shared_ptr<DrObjectBase>;
    auto obj = PtrType(DrClassMap::newObject("TestA"));
    EXPECT_NE(obj, nullptr);

    auto objPtr = DrClassMap::getSingleInstance("TestA");
    EXPECT_NE(objPtr.get(), nullptr);

    auto objPtr2 = DrClassMap::getSingleInstance<TestA>();
    EXPECT_NE(objPtr2.get(), nullptr);
    EXPECT_EQ(objPtr, objPtr2);
}

TEST(DrObjectTest, namespaceTest)
{
    using PtrType = std::shared_ptr<DrObjectBase>;
    auto obj = PtrType(DrClassMap::newObject("test::TestB"));
    EXPECT_NE(obj, nullptr);

    auto objPtr = DrClassMap::getSingleInstance("test::TestB");
    EXPECT_NE(objPtr.get(), nullptr);

    auto objPtr2 = DrClassMap::getSingleInstance<test::TestB>();
    EXPECT_NE(objPtr2.get(), nullptr);
    EXPECT_EQ(objPtr, objPtr2);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
