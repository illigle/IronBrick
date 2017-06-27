#include "gtest/gtest.h"
#include "IrkThread.h"
#include "IrkThreadPool.h"
#include "IrkOnExit.h"

using namespace irk;

static void accum( const int* data, int cnt, std::atomic_int* total )
{
    int sum = 0;
    for( int i = 0; i < cnt; i++ )
        sum += data[i];
    *total += sum;
}

TEST( ThreadPool, ThreadPool )
{
    ThreadPool thrPool;
    bool rst = thrPool.setup();
    ASSERT_TRUE( rst );

    // run waitable tasks
    std::atomic_int cnt{0};
    auto task1 = make_waitable_task( [&cnt]()
    {
        OSThread::sleep( 2 );
        cnt += 2;
    });
    auto task2 = make_waitable_task( [&cnt]()
    {
        OSThread::sleep( 1 );
        cnt += 1;
    });
    thrPool.run_task( task1 );
    thrPool.run_task( task2 );
    task1->wait();
    task2->wait();
    EXPECT_EQ( 0, thrPool.pending_count() );
    EXPECT_EQ( 3, cnt.load() );

    // test task group
    const int kSize = 1024 * 1024 * 16 + 64;
    const int kSeg = 1024;
    const int kVal = 15;
    int* data = new int[kSize];
    memset32( data, kVal, kSize );
    ON_EXIT( delete[] data );

    TaskGroup taskGrp( &thrPool );
    std::atomic_int sum{0};
    std::atomic_int* out = &sum;
    for( int idx = 0; idx < kSize; idx += kSeg )
    {
        const int* seg = data + idx;
        const int len = ((kSize - idx) >= kSeg) ? kSeg : (kSize - idx);
        taskGrp.run( [=]{ accum(seg, len, out); } );
    } 
    taskGrp.wait();
    EXPECT_EQ( kVal * kSize, sum.load() );
}
