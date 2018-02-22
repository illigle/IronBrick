#include <thread>
#include <chrono>
#include "gtest/gtest.h"
#include "IrkSpinMutex.h"

using namespace irk;
namespace chrono = std::chrono;

namespace {
struct Counter
{
    int x;
    int y;
};
}

TEST(Spin, SpinMutex)
{
    SpinMutex mutex;
    const int loop = 20;
    Counter cnt = {};

    auto worker = [&] {
        for (int i = 0; i < loop; i++)
        {
            SpinMutex::Guard gd_(mutex);
            int cx = cnt.x++;
            if (i & 1)
                std::this_thread::sleep_for(chrono::milliseconds(1));
            cnt.y = cx + 1;
        }
    };

    std::thread th1(worker);
    std::thread th2(worker);
    std::thread th3(worker);
    th1.join();
    th2.join();
    th3.join();

    EXPECT_EQ(3 * loop, cnt.x);
    EXPECT_EQ(cnt.x, cnt.y);
}

TEST(Spin, SharedMutex)
{
    SpinSharedMutex mutex;
    const int loop = 20;
    int readCnt = 0;
    Counter cnt = {};

    auto writer = [&] {
        for (int i = 0; i < loop; i++)
        {
            SpinSharedMutex::Guard gd_(mutex);
            cnt.x++;
            if (i & 1)
                std::this_thread::sleep_for(chrono::milliseconds(1));
            cnt.y++;
        }
    };
    auto reader = [&] {
        for (int i = 0; i < loop; i++)
        {
            SpinSharedMutex::SharedGuard sgd_(mutex);
            int cx = cnt.x;
            atomic_cpu_pause();
            int cy = cnt.y;
            EXPECT_EQ(cx, cy);

            int c = readCnt;
            if (i & 1)
                std::this_thread::sleep_for(chrono::milliseconds(1));
            readCnt = c + 1;
        }
    };

    std::thread th1(writer);
    std::thread th2(reader);
    std::thread th3(reader);
    th1.join();
    th2.join();
    th3.join();

    EXPECT_EQ(loop, cnt.x);
    EXPECT_EQ(loop, cnt.y);
    EXPECT_GE(loop * 2, readCnt);     // readCnt <= loop * 2
}

TEST(Spin, FiFoMutex)
{
    SpinFifoMutex mutex;
    const int loop = 20;
    Counter cnt = {};

    auto worker = [&] {
        FifoMxWaiter waiter;
        for (int i = 0; i < loop; i++)
        {
            SpinFifoMutex::Guard gd_(mutex, waiter);
            int cx = cnt.x++;
            if (i & 1)
                std::this_thread::sleep_for(chrono::milliseconds(1));
            cnt.y = cx + 1;
        }
    };

    std::thread th1(worker);
    std::thread th2(worker);
    std::thread th3(worker);
    th1.join();
    th2.join();
    th3.join();

    EXPECT_EQ(3 * loop, cnt.x);
    EXPECT_EQ(cnt.x, cnt.y);
}
