#include <random>
#include <map>
#include <set>
#include <string>
#include <limits.h>
#include "gtest/gtest.h"
#include "IrkFlatMap.h"

namespace {

struct Item
{
    Item() : x(0), y(1) {}
    Item(int a, int b) : x(a), y(b) {}
    int x;
    int y;
};

inline bool operator==(const Item& m1, const Item& m2)
{
    return m1.x == m2.x && m1.y == m2.y;
}
inline bool operator<(const Item& m1, const Item& m2)
{
    return (m1.x < m2.x) || (m1.x == m2.x) && (m1.y < m2.y);
}
inline bool operator>(const Item& m1, const Item& m2)
{
    return !(m1 < m2) && !(m1 == m2);
}

struct BigItem
{
    BigItem() : tag(0) {}
    BigItem(int t) : tag(t) {}
    int tag;
    uint8_t buf[256];
};
inline bool operator==(const BigItem& m1, const BigItem& m2)
{
    return m1.tag == m2.tag;
}
inline bool operator<(const BigItem& m1, const BigItem& m2)
{
    return m1.tag < m2.tag;
}
inline bool operator>(const BigItem& m1, const BigItem& m2)
{
    return m1.tag > m2.tag;
}

}

TEST(Container, FlatMap)
{
    std::random_device rdev;
    std::minstd_rand rnd(rdev());

    irk::FlatMap<int, Item> fmap(10);
    EXPECT_TRUE(fmap.empty());
    EXPECT_EQ(0, fmap.size());

    for (int i = 0; i < 30; i++)
    {
        fmap.insert_or_assign(i, Item{i + 1, i + 2});
    }
    EXPECT_FALSE(fmap.empty());
    EXPECT_EQ(30, fmap.size());

    for (int i = 0; i < 30; i++)
    {
        EXPECT_EQ(i, fmap.indexof(i));
        EXPECT_EQ(i, fmap.key(i));
        EXPECT_EQ(i + 1, fmap[i].x);
    }

    Item dummy;
    EXPECT_TRUE(fmap.contains(1));
    EXPECT_FALSE(fmap.contains(-1));
    EXPECT_FALSE(fmap.get(-100, &dummy));
    EXPECT_TRUE(fmap.remove(1));
    EXPECT_FALSE(fmap.remove(-1));
    fmap.erase(0);
    EXPECT_TRUE(2 == fmap.key(0) && 4 == fmap[0].y);

    fmap.clear();
    fmap.reset(0);
    EXPECT_TRUE(fmap.empty());
    EXPECT_EQ(0, fmap.size());

    std::map<int, Item> stdmap;
    for (int i = 0; i < 20; i++)
    {
        int x = (int)(rnd() - INT_MAX);
        stdmap.emplace(x, Item{x, x * 2});
        fmap.insert_or_assign(x, Item{x, x * 2});
    }
    int k = 0;
    for (const auto& item : stdmap)
    {
        EXPECT_EQ(item.first, fmap.key(k));
        EXPECT_EQ(item.second, fmap[k]);
        k++;
    }

    irk::FlatMap<const char*, BigItem> fatMap(0);
    fatMap.reserve(10);
    fatMap.insert_or_assign("11", BigItem{11});
    fatMap.insert_or_assign("44", BigItem{44});
    fatMap.insert_or_assign("22", BigItem{22});
    fatMap.insert_or_assign("33", BigItem{33});
    EXPECT_EQ(0, fatMap.indexof("11"));
    EXPECT_STREQ("22", fatMap.key(1));
    BigItem btm;
    EXPECT_TRUE(fatMap.get("44", &btm));
    EXPECT_EQ(44, btm.tag);
}

TEST(Container, FlatSet)
{
    std::random_device rdev;
    std::minstd_rand rnd(rdev());

    irk::FlatSet<int> fset(10);
    EXPECT_TRUE(fset.empty());
    EXPECT_EQ(0, fset.size());

    for (int i = 0; i < 30; i++)
    {
        fset.insert(i + 1);
    }
    EXPECT_FALSE(fset.empty());
    EXPECT_EQ(30, fset.size());

    for (int i = 0; i < 30; i++)
    {
        EXPECT_EQ(i, fset.indexof(i + 1));
        EXPECT_EQ(i + 1, fset[i]);
    }

    EXPECT_TRUE(fset.contains(1));
    EXPECT_FALSE(fset.contains(-1));
    EXPECT_TRUE(fset.remove(1));
    EXPECT_FALSE(fset.remove(-1));
    fset.erase(0);
    EXPECT_EQ(3, fset[0]);

    fset.clear();
    fset.reset(0);
    EXPECT_TRUE(fset.empty());
    EXPECT_EQ(0, fset.size());

    std::set<int> stdset;
    for (int i = 0; i < 20; i++)
    {
        int x = (int)(rnd() - INT_MAX);
        stdset.insert(x);
        fset.insert(x);
    }
    int k = 0;
    for (const auto& val : stdset)
    {
        EXPECT_EQ(val, fset[k]);
        k++;
    }

    irk::FlatSet<BigItem> fatSet(0);
    fatSet.reserve(10);
    fatSet.insert(BigItem{11});
    fatSet.insert(BigItem{44});
    fatSet.insert(BigItem{22});
    fatSet.insert(BigItem{33});
    EXPECT_EQ(22, fatSet[1].tag);
}
