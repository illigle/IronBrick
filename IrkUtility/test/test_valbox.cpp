#include <limits.h>
#include <memory>
#include <string>
#include "gtest/gtest.h"
#include "IrkValBox.h"

namespace {

struct MyValue
{
    MyValue(int x, int y, int z) : m_x(x), m_y(y), m_z(z) {}
    int m_x;
    int m_y;
    int m_z;
};
class Counter
{
public:
    explicit Counter(int& x) : m_refx(x) {}
    ~Counter() { m_refx++; }
private:
    int& m_refx;
};
}

using namespace irk;

TEST(ValBox, Normal)
{
    ValBox box;
    EXPECT_TRUE(box.empty());
    EXPECT_FALSE(box.has_value());
    EXPECT_FALSE(box.can_get<int>());
    box = 100;
    EXPECT_FALSE(box.empty());
    EXPECT_TRUE(box.has_value());
    EXPECT_TRUE(box.can_get<int>());
    int v = 0;
    EXPECT_TRUE(box.try_get<int>(&v));
    EXPECT_EQ(100, v);

    {
        ValBox box(false);
        EXPECT_EQ(false, box.get<bool>());
        box = true;
        EXPECT_EQ(true, box.get<bool>());
    }
    {
        ValBox box('c');
        EXPECT_EQ('c', box.get<char>());
        box = 'd';
        EXPECT_EQ('d', box.get<char>());
    }
    {
        int8_t x = -10;
        ValBox box(x);
        EXPECT_EQ(x, box.get<int8_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<int8_t>());
    }
    {
        uint8_t x = 10;
        ValBox box(x);
        EXPECT_EQ(x, box.get<uint8_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<uint8_t>());
    }
    {
        int16_t x = -32767;
        ValBox box(x);
        EXPECT_EQ(x, box.get<int16_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<int16_t>());
    }
    {
        uint16_t x = 32768;
        ValBox box(x);
        EXPECT_EQ(x, box.get<uint16_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<uint16_t>());
    }
    {
        int32_t x = -1000000;
        ValBox box(x);
        EXPECT_EQ(x, box.get<int32_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<int32_t>());
    }
    {
        uint32_t x = 1000000;
        ValBox box(x);
        EXPECT_EQ(x, box.get<uint32_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<uint32_t>());
    }
    {
        int64_t x = INT64_MIN;
        ValBox box(x);
        EXPECT_EQ(x, box.get<int64_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<int64_t>());
    }
    {
        uint64_t x = INT64_MAX;
        ValBox box(x);
        EXPECT_EQ(x, box.get<uint64_t>());
        box = x + 1;
        EXPECT_EQ(x + 1, box.get<uint64_t>());
    }
    {
        float x = -10.123f;
        ValBox box(x);
        ASSERT_EQ(x, box.get<float>());
        box = (x += 0.4561f);
        ASSERT_FLOAT_EQ(x, box.get<float>());
    }
    {
        double x = 99.9999;
        ValBox box(x);
        ASSERT_EQ(x, box.get<double>());
        box = (x += 0.00001);
        ASSERT_DOUBLE_EQ(x, box.get<double>());
    }
    {
        Rational r = {1, 2};
        ValBox box(r);
        EXPECT_EQ(r, box.get<Rational>());
        box = Rational{1, 3};
        EXPECT_EQ((Rational{1, 3}), box.get<Rational>());
    }
    {
        const char* str = "abc";
        ValBox box(str);
        EXPECT_STREQ(str, box.get<char*>());
        box = "123";
        EXPECT_STREQ("123", box.get<char*>());
    }
    {
        ValBox box(MyValue(1, 2, 3));
        MyValue v = box.get<MyValue>();
        EXPECT_TRUE(v.m_x == 1 && v.m_y == 2 && v.m_z == 3);
        v.m_x = -1;
        box = v;
        box.try_get<MyValue>(&v);
        EXPECT_TRUE(v.m_x == -1 && v.m_y == 2 && v.m_z == 3);

        const MyValue* pv = box.getptr<MyValue>();
        ASSERT_NE(nullptr, pv);
        EXPECT_TRUE(pv->m_x == -1 && pv->m_y == 2 && pv->m_z == 3);
    }
    {
        ValBox box = make_valuebox<MyValue>(1, 2, 3);
        EXPECT_TRUE(box.can_get<MyValue>());
        MyValue v = box.get<MyValue>();
        EXPECT_TRUE(v.m_x == 1 && v.m_y == 2 && v.m_z == 3);

        box.emplace<std::string>("box");
        std::string str = box.get<std::string>();
        EXPECT_TRUE(str.compare("box") == 0);

        box.emplace<bool>(true);
        EXPECT_TRUE(box.get<bool>());

        box.emplace<Rational>(11, 2);
        EXPECT_EQ(11, box.get<Rational>().num);
    }
}

TEST(ValBox, Abnormal)
{
    uint8_t v = UINT8_MAX;
    ValBox box(v);
    EXPECT_THROW(box.get<bool>(), BoxBadAccess);
    EXPECT_THROW(box.get<int8_t>(), BoxBadAccess);
    EXPECT_THROW(box.get<Rational>(), BoxBadAccess);
    EXPECT_THROW(box.get<MyValue>(), BoxBadAccess);
    // integer safe conversion
    EXPECT_EQ(v, box.get<uint8_t>());
    EXPECT_EQ(v, box.get<uint16_t>());
    EXPECT_EQ(v, box.get<uint32_t>());
    EXPECT_EQ(v, box.get<uint64_t>());
    EXPECT_EQ((int16_t)v, box.get<int16_t>());
    EXPECT_EQ((int32_t)v, box.get<int32_t>());
    EXPECT_EQ((int64_t)v, box.get<int64_t>());

    int i32 = 127;
    box = i32;
    EXPECT_FALSE(box.can_get<bool>());
    EXPECT_FALSE(box.can_get<Rational>());
    EXPECT_FALSE(box.can_get<MyValue>());
    EXPECT_TRUE(box.can_get<float>());        // convert int to float
    EXPECT_TRUE(box.can_get<double>());       // convert int to double
    // integer safe conversion
    EXPECT_EQ(i32, box.get<char>());
    EXPECT_EQ(i32, box.get<uint8_t>());
    EXPECT_EQ(i32, box.get<uint16_t>());
    EXPECT_EQ(i32, box.get<uint32_t>());
    EXPECT_EQ(i32, box.get<uint64_t>());
    EXPECT_EQ(i32, box.get<int16_t>());
    EXPECT_EQ(i32, box.get<int32_t>());
    EXPECT_EQ(i32, box.get<int64_t>());

    // integer safe conversion
    i32 = 65535;
    box = i32;
    EXPECT_FALSE(box.can_get<char>());
    EXPECT_FALSE(box.can_get<int8_t>());
    EXPECT_FALSE(box.can_get<uint8_t>());
    EXPECT_FALSE(box.can_get<int16_t>());
    EXPECT_EQ(i32, box.get<uint16_t>());
    EXPECT_EQ(i32, box.get<uint32_t>());
    EXPECT_EQ(i32, box.get<uint64_t>());
    EXPECT_EQ(i32, box.get<int32_t>());
    EXPECT_EQ(i32, box.get<int64_t>());
    EXPECT_FLOAT_EQ((float)i32, box.get<float>());
    EXPECT_DOUBLE_EQ((double)i32, box.get<double>());

    // float -> double
    float f32 = 1.23456f;
    box = f32;
    EXPECT_FALSE(box.can_get<bool>());
    EXPECT_FALSE(box.try_get<int>(&i32));
    EXPECT_FLOAT_EQ(f32, box.get<float>());
    EXPECT_DOUBLE_EQ(f32, box.get<double>());

    // destructor
    int cnt = 1;
    Counter cc(cnt);
    box = cc;
    box.reset();
    EXPECT_EQ(2, cnt);

    // move only data
    std::unique_ptr<MyValue> uptr = std::make_unique<MyValue>(1, 10, 100);
    box = std::move(uptr);
    EXPECT_FALSE(uptr);
    uptr = std::move(box).get<std::unique_ptr<MyValue>>();
    EXPECT_TRUE(uptr->m_x == 1 && uptr->m_y == 10 && uptr->m_z == 100);
}

TEST(ValBox, CopyMove)
{
    ValBox box1(1);
    ValBox box2 = box1;
    EXPECT_EQ(box1.get<int>(), box2.get<int>());

    ValBox box3 = make_valuebox<std::string>("123");
    box1 = box3;
    EXPECT_EQ(box3.get<std::string>(), box1.get<std::string>());
    EXPECT_EQ(std::string("123"), box1.get<std::string>());

    std::string str;
    ValBox box4(std::move(box3));
    box4.try_get<std::string>(&str);
    EXPECT_STREQ("123", str.c_str());
    EXPECT_THROW(box3.get<std::string>(), BoxBadAccess);

    ValBox box5(std::make_unique<MyValue>(1, 10, 100));
    EXPECT_THROW((box1 = box5), ContractViolation);       // can NOT copy
    box1 = std::move(box5);
    auto uptr = std::move(box1).get<std::unique_ptr<MyValue>>();
    EXPECT_TRUE(uptr->m_x == 1 && uptr->m_y == 10 && uptr->m_z == 100);

    ValBox box6(-1);
    ValBox box7("abc");
    box6.swap(box7);
    EXPECT_EQ(-1, box7.get<int>());
    EXPECT_STREQ("abc", box6.get<char*>());
}
