#include <vector>
#include "gtest/gtest.h"
#include "IrkRecycleBin.h"

namespace {

struct Counter
{
    Counter(int& x, int tag) : m_refx(x), m_tag(tag) {}
    ~Counter() { ++m_refx; }
    int tag() const { return m_tag; }
private:
    int& m_refx;
    int  m_tag;
};
}

using namespace irk;

TEST(RecycleBin, unsafe)
{
    RecycleBin<int, std::default_delete<int[]>> rcbin(8);
    EXPECT_TRUE(rcbin.is_empty());
    EXPECT_FALSE(rcbin.is_full());
    EXPECT_EQ(0, rcbin.count());

    int* buf1 = new int[64];
    int* buf2 = new int[128];
    rcbin.dump(buf1);
    buf1 = nullptr;
    rcbin.dump(buf2);
    buf2 = nullptr;
    EXPECT_FALSE(rcbin.is_empty());
    EXPECT_EQ(2, rcbin.count());

    buf2 = rcbin.getback();
    buf1 = rcbin.getback();
    EXPECT_NE(nullptr, buf1);
    EXPECT_NE(nullptr, buf2);
    EXPECT_EQ(nullptr, rcbin.getback());
    EXPECT_TRUE(rcbin.is_empty());

    rcbin.dump(buf1);
    rcbin.dump(buf2);
    rcbin.clear();
    EXPECT_TRUE(rcbin.is_empty());
    rcbin.reset();

    int val = 0;
    RecycleBin<Counter> rcbin2(0);
    Counter* cc = new Counter(val, -1);
    rcbin2.dump(cc);
    EXPECT_EQ(1, val);
}

TEST(RecycleBin, synced)
{
    int val = 0;

    do
    {
        SyncedRecycleBin<Counter> rcbin(8);
        EXPECT_TRUE(rcbin.is_empty());
        EXPECT_FALSE(rcbin.is_full());
        EXPECT_EQ(0, rcbin.count());

        int tag1 = 100, tag2 = 200;
        Counter* c1 = new Counter(val, tag1);
        Counter* c2 = new Counter(val, tag2);
        rcbin.dump(c1);
        c1 = nullptr;
        rcbin.dump(c2);
        c2 = nullptr;
        EXPECT_FALSE(rcbin.is_empty());
        EXPECT_EQ(2, rcbin.count());

        c2 = rcbin.getback();
        c1 = rcbin.getback();
        EXPECT_EQ(100, c1->tag());
        EXPECT_EQ(200, c2->tag());
        EXPECT_EQ(nullptr, rcbin.getback());

        rcbin.dump(c1);
        rcbin.dump(c2);
        rcbin.clear();

        int tag3 = 100, tag4 = 200;
        Counter* c3 = new Counter(val, tag3);
        Counter* c4 = new Counter(val, tag4);
        rcbin.dump(c3);
        rcbin.dump(c4);
    }
    while (0);
    EXPECT_EQ(4, val);

    SyncedRecycleBin<Counter> rcbin2(4);
    rcbin2.reset();
    Counter* cc = new Counter(val, -1);
    rcbin2.dump(cc);
    EXPECT_EQ(5, val);

}