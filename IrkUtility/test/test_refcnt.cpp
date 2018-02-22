#include "gtest/gtest.h"
#include "IrkRefCntObj.h"

using namespace irk;

namespace {

class Task : public RefCntObj
{
public:
    Task(int x, int y) : m_x(x), m_y(y) {}
    virtual ~Task() {}
    virtual int sum() const { return m_x + m_y; }
private:
    int m_x;
    int m_y;
};

class Task2 : public Task
{
public:
    Task2(int x, int y, int z) : Task(x, y), m_z(z) {}
    virtual int sum() const { return Task::sum() + m_z; }
private:
    int m_z;
};

}

TEST(RefCntObj, RefPtr)
{
    RefPtr<Task> rptr;
    EXPECT_FALSE(rptr);
    EXPECT_EQ(nullptr, rptr.pointer());

    RefPtr<Task> rptr2(new Task{100, 200});
    EXPECT_TRUE(rptr2);
    EXPECT_NE(nullptr, rptr2.pointer());

    rptr.swap(rptr2);
    EXPECT_TRUE(rptr);
    EXPECT_FALSE(rptr2);

    RefPtr<Task> rptr3(rptr);
    EXPECT_EQ(2, rptr3->ref_count());

    rptr2 = std::move(rptr3);
    EXPECT_TRUE(rptr2);
    EXPECT_FALSE(rptr3);
    EXPECT_EQ(2, rptr->ref_count());

    rptr2.rebind(nullptr);
    EXPECT_FALSE(rptr2);
    EXPECT_EQ(1, rptr->ref_count());

    auto task = make_refptr<Task>(-1, -2);
    EXPECT_TRUE(task);
    EXPECT_EQ(1, task->ref_count());
    task = rptr;
    EXPECT_EQ(2, (*task).ref_count());
    EXPECT_EQ(300, (*task).sum());

    auto task2 = make_refptr<Task2>(-1, -2, -3);
    EXPECT_EQ(-6, task2->sum());

    task = task2;   // assign to RefPtr of base class
    EXPECT_EQ(-6, task->sum());
    task.reset();
    EXPECT_TRUE(!task);
    EXPECT_EQ(1, task2->ref_count());

    RefPtr<Task> task3(new Task(0, 0));
    task3 = std::move(task2);
    EXPECT_TRUE(!task2);
    EXPECT_EQ(1, task3->ref_count());
    EXPECT_EQ(-6, task3->sum());
}
