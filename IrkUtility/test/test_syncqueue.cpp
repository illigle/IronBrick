#include <thread>
#include <string>
#include "gtest/gtest.h"
#include "IrkSyncQueue.h"

namespace chrono = std::chrono;
using namespace irk;

TEST(SyncQueue, Unbounded)
{
    const int cnt = 10;
    SyncedQueue<std::string> queue(cnt);
    EXPECT_TRUE(queue.is_empty());
    EXPECT_EQ(0, queue.count());

    auto sender = [&]
    {
        for (int i = 0; i < cnt; i++)
        {
            queue.push_back(std::string("abc"));
            std::this_thread::sleep_for(chrono::milliseconds(1));
        }
        for (int i = 0; i < cnt; i++)
        {
            queue.emplace_back(1, '0' + i);
            std::this_thread::sleep_for(chrono::milliseconds(1));
        }
        queue.emplace_back("exit");
    };
    auto receiver = [&]
    {
        std::string str;
        int k = 0;
        while (1)
        {
            if (queue.peek_front(&str))
            {
                if (str.compare("exit") == 0)
                    break;

                if (k < 10)
                    EXPECT_STREQ("abc", str.c_str());
                else
                    EXPECT_EQ(k - 10, str[0] - '0');
                k++;
                queue.pop_front();
            }
            else
            {
                std::this_thread::yield();
            }
        }
        EXPECT_EQ(cnt * 2, k);
    };

    std::thread th1(receiver);
    std::thread th2(sender);
    th1.join();
    th2.join();

    EXPECT_EQ(1, queue.count());
    std::string str;
    queue.pop_front(&str);
    EXPECT_STREQ("exit", str.c_str());

    queue.clear();
}

TEST(SyncQueue, Waitable)
{
    const int queueSize = 3;

    do
    {
        WaitableQueue<std::string> queue(queueSize);
        EXPECT_TRUE(queue.is_empty());
        EXPECT_FALSE(queue.is_full());
        EXPECT_FALSE(queue.is_closed());
        EXPECT_EQ(0, queue.count());

        // push 5 item
        std::string str("abc"), str2;
        EXPECT_TRUE(queue.push_back(str));
        EXPECT_EQ(WaitStatus::Ok, queue.push_back_wait(str));
        EXPECT_EQ(WaitStatus::Ok, queue.push_back_wait_for(str, 30));
        EXPECT_EQ(WaitStatus::Timeout, queue.push_back_wait_for(str, 30));
        EXPECT_TRUE(queue.is_full());
        EXPECT_FALSE(queue.is_closed());
        EXPECT_EQ(3, queue.count());
        EXPECT_TRUE(queue.force_push_front(std::string("ABC")));
        EXPECT_TRUE(queue.force_push_back(str));
        EXPECT_TRUE(queue.is_full());
        EXPECT_EQ(5, queue.count());

        // pop
        EXPECT_TRUE(queue.pop_front(&str2));
        EXPECT_STREQ("ABC", str2.c_str());
        EXPECT_TRUE(queue.pop_front(&str2));
        EXPECT_EQ(str, str2);
        EXPECT_EQ(WaitStatus::Ok, queue.pop_front_wait(&str2));
        EXPECT_EQ(str, str2);
        EXPECT_EQ(WaitStatus::Ok, queue.pop_front_wait_for(&str2, 30));
        EXPECT_EQ(str, str2);
        EXPECT_TRUE(queue.pop_front(&str2));
        EXPECT_EQ(str, str2);
        EXPECT_TRUE(queue.is_empty());
        EXPECT_FALSE(queue.is_full());
        EXPECT_FALSE(queue.is_closed());
        EXPECT_EQ(0, queue.count());
        EXPECT_FALSE(queue.pop_front());
        EXPECT_EQ(WaitStatus::Timeout, queue.pop_front_wait_for(&str2, 30));

        // close
        EXPECT_TRUE(queue.push_back(std::move(str2)));
        queue.close();
        EXPECT_TRUE(queue.is_closed());
        EXPECT_EQ(1, queue.count());
        EXPECT_FALSE(queue.push_back(str));
        EXPECT_FALSE(queue.force_push_back(std::string("ABC")));
        EXPECT_FALSE(queue.force_push_front(str));
        EXPECT_EQ(WaitStatus::Closed, queue.push_back_wait(str));
        EXPECT_EQ(WaitStatus::Closed, queue.push_back_wait_for(str, 30));
        EXPECT_EQ(1, queue.count());
        EXPECT_TRUE(queue.pop_front());
        EXPECT_TRUE(queue.is_empty());
    }
    while (0);

    do
    {
        WaitableQueue<int> queue2(queueSize);
        auto sender = [&]
        {
            for (int i = 0; i < 10; i++)
            {
                queue2.push_back(i);
                std::this_thread::sleep_for(chrono::milliseconds(2));
            }
            queue2.close();
        };
        auto receiver = [&]
        {
            int val = 0, k = 0;
            while (1)
            {
                auto res = queue2.pop_front_wait_for(&val, 1);
                if (res == WaitStatus::Ok)
                {
                    EXPECT_EQ(k, val);
                    k++;
                }
                else if (res == WaitStatus::Timeout)
                {
                    continue;
                }
                else    // queue closed
                {
                    EXPECT_EQ(WaitStatus::Closed, res);
                    break;
                }
            }
        };
        std::thread th1(receiver);
        std::thread th2(sender);
        th1.join();
        th2.join();
    }
    while (0);
}
