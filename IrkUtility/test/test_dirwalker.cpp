#include "gtest/gtest.h"
#include "IrkEnvUtility.h"
#include "IrkDirWalker.h"

using namespace irk;

TEST( DirWalker, DirWalker )
{
    std::string homeDir = get_home_dir();
    DirWalker dwk( homeDir.c_str() );
    EXPECT_TRUE( dwk );

    int count = 0;
    for( const DirEntry* entry : dwk )
        count++;
    EXPECT_TRUE( count > 0 );

    int count2 = 0;
    dwk.rewind();
    while( dwk.next_entry() )
        count2++;
    EXPECT_EQ( count, count2 );

    DirWalker dwk2( "/thisdirisnoexists!@#$%" );
    EXPECT_FALSE( dwk2 );
    EXPECT_FALSE( dwk2.is_opened() );

#ifdef _WIN32
    int count3 = 0;
    for( auto entry : DirWalkerW( get_home_dir_ws().c_str() ) )
        count3++;
    EXPECT_EQ( count, count3 );
#endif
}
