#include <stdio.h>
#include "gtest/gtest.h"
#include "IrkDnsResolver.h"

using namespace irk;

static void enum_addrlist( const char* host, const AddrInfoList& addrlist )
{
    if( addrlist.empty() )
    {
        printf( "resolve %s failed: %s\n", host, resolve_strerror(addrlist.error()) );
        return;
    }

    EXPECT_FALSE( addrlist.empty() );

    for( auto& addr : addrlist )
    {
        SocketAddr sa = make_sockaddr( addr );
        printf( "family=%d, socktype=%d, addr=%s\n", addr.ai_family, addr.ai_socktype, sa.to_string().c_str() );
    }
}

TEST( DnsResolver, DnsResolver )
{
    network_setup();

    const char* host = "www.baidu.com";
    const char* service = "http";

    auto addrlist = resolve_host( host, service ); 
    enum_addrlist( host, addrlist );

    if( !addrlist.empty() )
    {
        std::string host2, service2;
        SocketAddr saddr = make_sockaddr( addrlist.data() );
        get_hostname( saddr, host2, service2, NI_NUMERICHOST );
        EXPECT_EQ( saddr.addressv4().to_string(), host2 );
    }

    addrlist = resolve_host( host, service, SOCK_STREAM, AF_INET6 );
    enum_addrlist( host, addrlist );
}
