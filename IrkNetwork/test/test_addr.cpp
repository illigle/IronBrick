#include <string>
#include "gtest/gtest.h"
#include "IrkIpAddress.h"

using namespace irk;

TEST( IPAddr, IPAddr )
{
    char buf[65];
    std::string str;

    Ipv4Addr addr = Ipv4Addr::make_loopback();
    EXPECT_TRUE( addr.is_loopback() );
    addr.to_string( buf, 16 );
    EXPECT_STREQ( "127.0.0.1", buf );

    addr.from_string( "224.0.0.1" );
    EXPECT_TRUE( addr.is_multicast() );
    str = addr.to_string();
    EXPECT_STREQ( "224.0.0.1", str.c_str() );

    Ipv6Addr addr6;
    EXPECT_TRUE( addr6.is_unspecified() );
    addr6.from_string( "::202" );
    str = addr6.to_string();
    EXPECT_STREQ( "::202", str.c_str() );

    addr6.from_native( in6addr_loopback );
    str = addr6.to_string();
    EXPECT_STREQ( "::1", str.c_str() );

    IpAddr addr1;
    IpAddr addr2( addr6 );
    addr1 = addr6;
    str = addr1.to_string();
    EXPECT_STREQ( "::1", str.c_str() );
    addr2.to_string( buf, 64 );
    EXPECT_STREQ( "::1", buf );

    SocketAddr saddr;
    EXPECT_TRUE( saddr.is_ipv4() && saddr.is_unspecified() );
    EXPECT_TRUE( saddr.addressv4() == Ipv4Addr::make_unspecified() );
    EXPECT_EQ( 16, saddr.length() );

    saddr.set( Ipv6Addr::make_loopback(), 2000 );
    EXPECT_TRUE( saddr.is_ipv6() );
    EXPECT_TRUE( saddr.port() == 2000 );
    EXPECT_TRUE( saddr.address().is_loopback() );
    str = saddr.to_string();
    EXPECT_STREQ( "[::1]:2000", str.c_str() );
    
    SocketAddr saddr2( "192.168.8.2", 8080 );
    EXPECT_TRUE( saddr2.is_ipv4() );
    EXPECT_TRUE( saddr2.port() == 8080 );
    saddr2.to_string( buf, 64 );
    EXPECT_STREQ( "192.168.8.2:8080", buf );
    str = saddr2.address().to_string();
    EXPECT_STREQ( "192.168.8.2", str.c_str() );
}
