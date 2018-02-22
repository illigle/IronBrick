#include "gtest/gtest.h"
#include "IrkQueue.h"

namespace {

struct Item
{
    Item(int a, int b) : x(a), y(b) {}
    int x;
    int y;
};
}

TEST(Container, Queue)
{
    int val = -1;
    irk::Queue<int> q(10);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(0, q.size());
    for (int v : q)
    {
        ASSERT_TRUE(false);
    }

    q.push_back(100);
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(1, q.size());

    q.push_back(200);
    q.pop_front();
    q.pop_front(&val);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(0, q.size());
    EXPECT_EQ(200, val);

    for (int i = 0; i < 30; i++)
    {
        q.push_back(i + 1);
    }
    EXPECT_EQ(30, q.size());

    for (int i = 0; i < 30; i++)
    {
        EXPECT_EQ(i + 1, q[i]);
    }

    for (int i = 0; i < 10; i++)
    {
        q.pop_front();
    }
    int k = 11;
    for (int& v : q)
    {
        EXPECT_EQ(k++, v);
        v = -100;
    }
    for (int v : q)
    {
        EXPECT_EQ(-100, v);
    }

    q.push_back(1000);
    EXPECT_EQ(-100, q.front());
    EXPECT_EQ(1000, q.back());

    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(0, q.size());

    // test for const queue
    for (int i = 0; i < 10; i++)
    {
        q.push_back(i + 1);
    }
    const auto& cq = q;
    k = 1;
    for (int v : cq)
    {
        EXPECT_EQ(k++, v);
    }
    EXPECT_EQ(1, cq.front());
    EXPECT_EQ(10, cq.back());

    irk::Queue<Item> itemQueue(10);
    itemQueue.push_back(Item(0, 1));
    for (auto& item : itemQueue)
    {
        EXPECT_TRUE(item.x == 0 && item.y == 1);
    }
}

