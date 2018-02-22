#include "gtest/gtest.h"
#include "IrkAtomic.h"

using namespace irk;

TEST(Atomic, Atomic)
{
    volatile int v = 1;
    volatile int64_t v64 = 100;
    void* volatile ptr = nullptr;
    EXPECT_EQ(1, atomic_load(&v));
    EXPECT_EQ(100, atomic_load(&v64));
    EXPECT_EQ(nullptr, atomic_load(&ptr));

    atomic_store(&v, 2);
    atomic_store(&v64, 200);
    atomic_store(&ptr, (void*)&v64);
    EXPECT_EQ(2, v);
    EXPECT_EQ(200, v64);
    EXPECT_EQ((void*)&v64, (void*)ptr);

    EXPECT_EQ(3, atomic_inc(&v));
    EXPECT_EQ(201, atomic_inc(&v64));
    EXPECT_EQ(2, atomic_dec(&v));
    EXPECT_EQ(200, atomic_dec(&v64));

    EXPECT_EQ(2, atomic_fetch_add(&v, 10));
    EXPECT_EQ(200, atomic_fetch_add(&v64, 10));
    EXPECT_EQ(12, v);
    EXPECT_EQ(210, v64);

    EXPECT_EQ(12, atomic_fetch_sub(&v, 8));
    EXPECT_EQ(210, atomic_fetch_sub(&v64, 194));
    EXPECT_EQ(4, v);
    EXPECT_EQ(16, v64);

    EXPECT_EQ(4, atomic_fetch_or(&v, 1));
    EXPECT_EQ(16, atomic_fetch_or(&v64, 1));
    EXPECT_EQ(5, v);
    EXPECT_EQ(17, v64);

    EXPECT_EQ(5, atomic_fetch_and(&v, 3));
    EXPECT_EQ(17, atomic_fetch_and(&v64, 3));
    EXPECT_EQ(1, v);
    EXPECT_EQ(1, v64);

    EXPECT_EQ(1, atomic_fetch_xor(&v, 5));
    EXPECT_EQ(1, atomic_fetch_xor(&v64, 5));
    EXPECT_EQ(4, v);
    EXPECT_EQ(4, v64);

    EXPECT_EQ(0, atomic_bit_fetch_set(&v, 1));
    EXPECT_EQ(0, atomic_bit_fetch_set(&v64, 1));
    EXPECT_EQ(6, v);
    EXPECT_EQ(6, v64);

    EXPECT_EQ(1, atomic_bit_fetch_reset(&v, 2));
    EXPECT_EQ(1, atomic_bit_fetch_reset(&v64, 2));
    EXPECT_EQ(2, v);
    EXPECT_EQ(2, v64);

    EXPECT_EQ(1, atomic_bit_fetch_complement(&v, 1));
    EXPECT_EQ(1, atomic_bit_fetch_complement(&v64, 1));
    EXPECT_EQ(0, v);
    EXPECT_EQ(0, v64);

    EXPECT_EQ(0, atomic_exchange(&v, 100));
    EXPECT_EQ(0, atomic_exchange(&v64, 10000));
    EXPECT_EQ(100, v);
    EXPECT_EQ(10000, v64);

    int vv = -100;
    int64_t vv64 = -10000;
    EXPECT_FALSE(atomic_compare_exchange(&v, &vv, 0));
    EXPECT_FALSE(atomic_compare_exchange(&v64, &vv64, 0));
    EXPECT_EQ(100, v);
    EXPECT_EQ(100, vv);
    EXPECT_EQ(10000, v64);
    EXPECT_EQ(10000, vv64);
    EXPECT_TRUE(atomic_compare_exchange(&v, &vv, 0));
    EXPECT_TRUE(atomic_compare_exchange(&v64, &vv64, 0));
    EXPECT_EQ(0, v);
    EXPECT_EQ(0, v64);

    EXPECT_EQ(0, atomic_compare_swap(&v, 0, -9));
    EXPECT_EQ(0, atomic_compare_swap(&v64, 0, -99));
    EXPECT_EQ(-9, v);
    EXPECT_EQ(-99, v64);
}
