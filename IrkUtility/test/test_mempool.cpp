#include <array>
#include "gtest/gtest.h"
#include "IrkMemPool.h"

namespace {

struct Pos2D
{
    int x;
    int y;
};

struct Pos3D
{
    int x;
    int y;
    int z;
};

struct Counter
{
    explicit Counter(int& x) : m_refx(x) { x++; }
    ~Counter() { m_refx++; }
private:
    int& m_refx;
};
}

using namespace irk;

TEST(MemPool, Slots)
{
    MemSlots slots(sizeof(Pos2D), 16, alignof(Pos2D));
    EXPECT_GE(slots.slot_size(), sizeof(Pos2D));

    void* pos = slots.alloc();
    EXPECT_NE(nullptr, pos);
    slots.dealloc(pos);
    void* pos2 = slots.alloc();
    EXPECT_EQ(pos, pos2);

    slots.init(sizeof(Pos3D), 16, alignof(Pos3D));
    EXPECT_GE(slots.slot_size(), sizeof(Pos3D));
    void* pos3 = slots.alloc(sizeof(Pos3D), alignof(Pos3D));
    slots.dealloc(pos3, sizeof(Pos3D));

    for (int i = 0; i < 50; i++)
    {
        Pos3D* pos = (Pos3D*)slots.alloc();
        pos->x = 1;
        pos->y = 2;
        pos->z = 3;
    }
    slots.clear();

    slots.init(sizeof(Counter), 16, alignof(Counter));
    int x = 10;
    Counter* cc = irk_new<Counter>(slots, x);
    EXPECT_EQ(11, x);
    irk_delete(slots, cc);
    EXPECT_EQ(12, x);
}

TEST(MemPool, Chunk)
{
    MemChunks chunk(sizeof(Pos2D) * 16);

    void* pos = chunk.alloc(sizeof(Pos2D), alignof(Pos2D));
    EXPECT_NE(nullptr, pos);
    chunk.dealloc(pos, sizeof(Pos2D));
    void* pos2 = chunk.alloc(sizeof(float) * 4, alignof(float));
    EXPECT_EQ(pos, pos2);

    chunk.init(sizeof(Pos3D) * 16);
    void* pos3 = chunk.alloc(sizeof(Pos3D), alignof(Pos3D));
    chunk.dealloc(pos3, sizeof(Pos3D));

    for (int i = 0; i < 50; i++)
    {
        Pos3D* pos = (Pos3D*)chunk.alloc(sizeof(Pos3D), alignof(Pos3D));
        pos->x = 1;
        pos->y = 2;
        pos->z = 3;
    }
    chunk.alloc(4096);

    int x = 10;
    Counter* cc = irk_new<Counter>(chunk, x);
    EXPECT_EQ(11, x);
    irk_delete(chunk, cc);
    EXPECT_EQ(12, x);
}

TEST(MemPool, Arena)
{
    std::array<uint8_t, 128> buf;
    MemArena arena(buf.data(), buf.size());

    void* pos = arena.alloc(sizeof(Pos2D), alignof(Pos2D));
    EXPECT_NE(nullptr, pos);
    arena.dealloc(pos, sizeof(Pos2D));
    void* pos2 = arena.alloc(sizeof(Pos2D), alignof(Pos2D));
    EXPECT_EQ(pos, pos2);
    arena.dealloc(pos2, sizeof(Pos2D));

    arena.init(buf.data(), buf.size());
    for (int i = 1; i <= 16; i++)
    {
        Pos3D* pos = (Pos3D*)arena.alloc(sizeof(Pos3D), alignof(Pos3D));
        pos->x = 1 * i;
        pos->y = 2 * i;
        pos->z = 3 * i;
        Pos3D* pos2 = (Pos3D*)arena.alloc(sizeof(Pos3D), alignof(Pos3D));
        pos2->x = 0x10 * i;
        pos2->y = 0x20 * i;
        pos2->z = 0x30 * i;
        arena.dealloc(pos2, sizeof(Pos3D));
        arena.dealloc(pos, sizeof(Pos3D));
    }

    int x = 10;
    Counter* cc = irk_new<Counter>(arena, x);
    EXPECT_EQ(11, x);
    irk_delete(arena, cc);
    EXPECT_EQ(12, x);
}

TEST(MemPool, Pool)
{
    MemPool pool(1024 * 16);

    void* pos = pool.alloc(sizeof(Pos2D), alignof(Pos2D));
    EXPECT_NE(nullptr, pos);
    pool.dealloc(pos, sizeof(Pos2D));
    void* pos2 = pool.alloc(sizeof(Pos2D), alignof(Pos2D));
    EXPECT_EQ(pos, pos2);

    pool.init(sizeof(Pos3D) * 16);
    void* pos3 = pool.alloc(sizeof(Pos3D), alignof(Pos3D));
    pool.dealloc(pos3, sizeof(Pos3D));

    for (int i = 0; i < 50; i++)
    {
        Pos3D* pos = (Pos3D*)pool.alloc(sizeof(Pos3D), alignof(Pos3D));
        pos->x = 1;
        pos->y = 2;
        pos->z = 3;
    }
    pool.alloc(4096);

    int x = 10;
    Counter* cc = irk_new<Counter>(pool, x);
    EXPECT_EQ(11, x);
    irk_delete(pool, cc);
    EXPECT_EQ(12, x);
}
