#include "gtest/gtest.h"
#include "IrkOnExit.h"

namespace {

struct Counter
{
    explicit Counter(int& x) : m_refx(x) {}
    ~Counter() { ++m_refx; }
private:
    int& m_refx;
};
}

TEST(OnExit, OnExit)
{
    int x = 10;
    {
        Counter* cc = new Counter(x);
        ON_EXIT(delete cc);
    }
    EXPECT_EQ(11, x);

    if (x > 0)
    {
        ON_EXIT(x += 1; x += 100);
    }
    EXPECT_EQ(112, x);
}
