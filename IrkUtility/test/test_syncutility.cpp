#include <thread>
#include <chrono>
#include "gtest/gtest.h"
#include "IrkSyncUtility.h"
#include "IrkThread.h"

using namespace irk;
namespace chrono = std::chrono;

namespace {
struct Counter
{
    int x;
    int y;
};

class CounterThread : public irk::OSThread
{
public:
    CounterThread(irk::Mutex& mx, Counter& cnt, int loop)
        : m_mutex(mx), m_cnt(cnt), m_loop(loop)
    {}
    void thread_proc() override
    {
        for (int i = 0; i < m_loop; i++)
        {
            irk::Mutex::Guard gd_(m_mutex);

            int cx = m_cnt.x++;
            if (i & 1)
                irk::OSThread::sleep(1);
            m_cnt.y = cx + 1;
        }
    }
private:
    Counter & m_cnt;
    int         m_loop;
    irk::Mutex& m_mutex;
};
}

TEST(Sync, Mutex)
{
    irk::Mutex mutex;
    Counter cnt = {};
    const int loop = 20;

    CounterThread th1(mutex, cnt, loop);
    CounterThread th2(mutex, cnt, loop);
    CounterThread th3(mutex, cnt, loop);
    th1.launch();
    th2.launch();
    th3.launch();
    th1.join();
    th2.join();
    th3.join();

    EXPECT_EQ(3 * loop, cnt.x);
    EXPECT_EQ(cnt.x, cnt.y);
}

TEST(Sync, SharedMutex)
{
    SharedMutex mutex;
    const int loop = 20;
    int readCnt = 0;
    Counter cnt = {};

    auto writer = [&] {
        for (int i = 0; i < loop; i++)
        {
            SharedMutex::Guard gd_(mutex);
            cnt.x++;
            if (i & 1)
                std::this_thread::sleep_for(chrono::milliseconds(1));
            cnt.y++;
        }
    };
    auto reader = [&] {
        for (int i = 0; i < loop; i++)
        {
            SharedMutex::SharedGuard sgd_(mutex);
            int cx = cnt.x;
            std::this_thread::yield();
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

TEST(Sync, CondVar)
{
    const int kQsize = 4, kCnt = kQsize * 10;
    int queue[kQsize] = {};
    int data[kCnt];
    for (int i = 0; i < kCnt; i++)
        data[i] = i * 2 + 1;

    int sum = 0;
    int curCnt = 0;
    Mutex mx;
    CondVar cvNotFull, cvNotEmpty;

    auto sender = [&]()
    {
        for (int i = 0; i < kCnt; i++)
        {
            Mutex::Guard guard(mx);

            while (curCnt >= kQsize)
                cvNotFull.wait(mx);
            queue[curCnt++] = data[i];

            guard.unlock();
            cvNotEmpty.notify_one();
        }
    };
    auto receiver = [&]()
    {
        for (int i = 0; i < kCnt; i++)
        {
            Mutex::Guard guard(mx);

            while (curCnt == 0)
                cvNotEmpty.wait(mx);
            curCnt--;
            sum += queue[curCnt];

            guard.unlock();
            cvNotFull.notify_one();
        }
    };

    std::thread th1(receiver);
    std::thread th2(sender);
    th1.join();
    th2.join();

    int sum2 = 0;
    for (int v : data)
        sum2 += v;
    EXPECT_EQ(sum2, sum);
}

TEST(Sync, Event)
{
    const int kCNT = 10;
    int data[kCNT] = {};
    int sum = 0;

    SyncEvent sendEvt(false);
    SyncEvent recvEvt(false);
    recvEvt.set();
    auto sender = [&]
    {
        for (int i = 0; i < kCNT; i++)
        {
            recvEvt.wait();
            data[i] = i + 1;
            sendEvt.set();
        }
    };
    auto receiver = [&]
    {
        for (int i = 0; i < kCNT; i++)
        {
            sendEvt.wait();
            sum += data[i];
            recvEvt.set();
        }
    };
    std::thread th1(receiver);
    std::thread th2(sender);
    th1.join();
    th2.join();
    EXPECT_EQ(55, sum);

    SyncEvent sendEvt2(true);
    auto sender2 = [&]
    {
        memset(data, 0, sizeof(data));
        for (int i = 0; i < kCNT; i++)
            data[i] = i + 1;
        sendEvt2.set();
    };
    auto receiver2 = [&]
    {
        sendEvt2.wait();
        sum = 0;
        for (int i = 0; i < kCNT; i++)
            sum += data[i];
    };
    std::thread th11(receiver2);
    std::thread th22(sender2);
    th11.join();
    th22.join();
    EXPECT_EQ(55, sum);
}

TEST(Sync, CountedEvent)
{
    const int cnt = 4;
    int workdata[cnt] = {0};
    std::thread threadAry[cnt];

    CounterEvent cevt;
    cevt.reset(cnt);
    auto worker = [&](int idx)
    {
        workdata[idx] = (idx + 1) * (idx + 1);
        std::this_thread::sleep_for(chrono::milliseconds(1));
        cevt.dec();
    };
    for (int i = 0; i < cnt; i++)
    {
        threadAry[i] = std::thread(worker, i);
    }

    cevt.wait();    // wait all worker thread done

    for (int i = 0; i < cnt; i++)
    {
        EXPECT_EQ(workdata[i], (i + 1) * (i + 1));
    }
    for (int i = 0; i < cnt; i++)
    {
        threadAry[i].join();
    }

    // reuse
    cevt.reset(cnt);
    cevt.dec(cnt - 1);
    EXPECT_FALSE(cevt.ready());
    cevt.dec_and_wait();
    EXPECT_TRUE(cevt.ready());
}
