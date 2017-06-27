#ifdef _WIN32
#include <windows.h>
#endif
#include <utility>
#include "gtest/gtest.h"
#include "IrkMemUtility.h"

using namespace irk;

namespace {

class Resource
{
public:
    static Resource* create( int& x ) 
    {
        return new Resource( x );
    }
    static void trash( Resource* r )
    {
        delete r;
    }
    void bark()
    {
        m_ref *= 3;
    }
private:
    explicit Resource( int& x ) : m_ref(x) { x *= 2; }
    ~Resource() { m_ref++; }
    int& m_ref;
};
}

TEST( MemUtility, AlignedBuf )
{
    AlignedBuf abuf;
    EXPECT_EQ( nullptr, abuf.buffer() );

    AlignedBuf abuf2( 4096, 16 );
    EXPECT_NE( nullptr, abuf2.buffer() );
    EXPECT_TRUE( ((uintptr_t)abuf2.buffer() & 15) == 0 );

    void* ptr2 = abuf2.detach();
    EXPECT_EQ( nullptr, abuf2.buffer() );
    EXPECT_NE( nullptr, ptr2 );
    EXPECT_TRUE( ((uintptr_t)ptr2 & 15) == 0 );
    abuf.rebind( ptr2 );
    EXPECT_EQ( ptr2, abuf.buffer() );

    AlignedBuf abuf3( std::move(abuf) );    // move construction
    EXPECT_EQ( nullptr, abuf.buffer() );
    EXPECT_EQ( ptr2, abuf3.buffer() );
    void* ptr3 = abuf3.alloc( 1024 * 16, 32 );  // extend
    EXPECT_TRUE( ((uintptr_t)ptr3 & 31) == 0 );

    abuf2 = std::move( abuf3 );             // move assignment
    EXPECT_EQ( nullptr, abuf3.buffer() );
    EXPECT_EQ( ptr3, abuf2.buffer() );
    abuf2.rebind();                         // dealloc
    EXPECT_EQ( nullptr, abuf2.buffer() );

    void* ptr4 = aligned_malloc( 1024 * 16, 16 );
    AlignedBuf abuf4( ptr4 );
    EXPECT_EQ( ptr4, abuf4.buffer() );
}

TEST( MemUtility, MallocBuf )
{
    MallocedBuf mbuf;
    EXPECT_EQ( nullptr, mbuf.buffer() );

    MallocedBuf mbuf2( 4096 );
    EXPECT_NE( nullptr, mbuf2.buffer() );

    void* ptr2 = mbuf2.detach();
    EXPECT_EQ( nullptr, mbuf2.buffer() );
    EXPECT_NE( nullptr, ptr2 );
    mbuf.rebind( ptr2 );
    EXPECT_EQ( ptr2, mbuf.buffer() );

    MallocedBuf mbuf3( std::move(mbuf) );       // move construction
    EXPECT_EQ( nullptr, mbuf.buffer() );
    EXPECT_EQ( ptr2, mbuf3.buffer() );
    void* ptr3 = mbuf3.alloc( 1024 * 16 );      // extend
    mbuf3.alloc( 1024 );                        // shrink
    EXPECT_EQ( ptr3, mbuf3.buffer() );

    mbuf2 = std::move( mbuf3 );                 // move assignment
    EXPECT_EQ( nullptr, mbuf3.buffer() );
    EXPECT_EQ( ptr3, mbuf2.buffer() );
    mbuf2.rebind();                             // dealloc
    EXPECT_EQ( nullptr, mbuf2.buffer() );
    mbuf2.calloc( 1024, 1 );
    EXPECT_EQ( 0, *(int32_t*)mbuf2.buffer() );

    void* ptr4 = checked_calloc( 1024, 16 );
    MallocedBuf mbuf4( ptr4 );
    EXPECT_EQ( ptr4, mbuf4.buffer() );
    mbuf4.realloc( 0 );
    mbuf4.realloc( 1024 );
    EXPECT_NE( nullptr, mbuf4.buffer() );
}

TEST( MemUtility, Utility )
{
#ifdef _WIN32
    do
    {
        HANDLE hEvent = ::CreateEvent( NULL, TRUE, FALSE, NULL );
        auto evt = make_unique_resource( hEvent, &::CloseHandle );
        EXPECT_EQ( hEvent, evt.get() );
    }
    while(0);
#endif

    int val = 10;
    int x = val;
    if( x != 0 )
    {
        Resource* res = Resource::create( x );
        auto r1 = make_unique_resource( res, &Resource::trash );
        r1->bark();
    }
    EXPECT_EQ( val*2*3+1, x );

    val = x;
    if( x != 0 )
    {
        Resource* res = Resource::create( x );
        auto r1 = make_shared_resource( res, [](Resource* r){ Resource::trash(r); } );
        r1->bark();
    }
    EXPECT_EQ( val*2*3+1, x );

    constexpr size_t cnt = 64;
    auto uary = make_unique_array<int32_t>( cnt );
    auto sary = make_shared_array<int16_t>( cnt );
    EXPECT_EQ( 0, uary[cnt-1] );
    EXPECT_EQ( 0, sary.get()[cnt-1] );

    auto uraw = make_unique_raw_array<int32_t>( cnt );
    auto sraw = make_shared_raw_array<int16_t>( cnt );
    EXPECT_NE( nullptr, uraw.get() );
    EXPECT_TRUE( sraw );

    memset32( uraw.get(), 0, cnt );
    EXPECT_EQ( 0, memcmp(uary.get(), uraw.get(), cnt*sizeof(int32_t)) );

    uint16_t buf[8];
    uint16_t* ptr = buf;
    memset16( buf, 0xABCD, countof(buf) );
    for( size_t i = 0; i < countof(buf); i++ )
        EXPECT_EQ( 0xABCD, buf[i] );
    EXPECT_EQ( ptr, buf );

    uint8_t buf4[7];
    uint8_t* ptr4 = buf4;
    secure_bzeros( buf4, countof(buf4) );
    for( size_t i = 0; i < countof(buf4); i++ )
        EXPECT_EQ( 0, buf4[i] );
    EXPECT_EQ( ptr4, buf4 );

    struct Position
    {
        int x; int y; int z;
    };
    Position pos;
    bzeros( &pos );
    EXPECT_TRUE( (pos.x | pos.y | pos.z) == 0 );

    const volatile uint64_t vbuf[24] = {};
    EXPECT_EQ( 24u, countof(vbuf) );
}
