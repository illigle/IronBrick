#include "IrkSocket.h"
#include "IrkStringUtility.h"
#include <memory>

using namespace irk;

static void launch_udp_server( const char* szIP, const char* szPort )
{
    constexpr int kBufsz = 16 * 1024;
    auto ubuf = std::make_unique<char[]>( kBufsz );
    const uint16_t port = (uint16_t)::strtol( szPort, NULL, 10 );

    UdpSocket skt;
    skt.open();
    skt.set_addr_reusable();
    skt.bind( szIP, port );
    if( !skt )
    {
        printf( "open udp socket failed\n" );
        return;
    }

    SocketAddr cliAddr;
    while( 1 )
    {
        // receive message
        int recved = 0;
        auto rst = skt.recvfrom( cliAddr, ubuf.get(), kBufsz, recved );
        if( !rst )
        {
            printf( "udp recv failed: %d\n", rst.errc );
            break;
        }
        else
        {
            ubuf[recved] = 0;
            printf( "udp recv from %s : %s\n", cliAddr.to_string().c_str(), ubuf.get() );
        }

        if( stricmp( ubuf.get(), "exit" ) == 0 )
            break;

        // echo back
        int sent = 0;
        rst = skt.sendto( cliAddr, ubuf.get(), recved, sent );
        if( !rst )
        {
            printf( "udp send failed: %d\n", rst.errc );
            break;
        }
    }
}

static void launch_tcp_server( const char* szIP, const char* szPort )
{
    constexpr int kBufsz = 16 * 1024;
    auto ubuf = std::make_unique<char[]>( kBufsz );
    const uint16_t port = (uint16_t)::strtol( szPort, NULL, 10 );

    TcpAcceptor acceptor;
    int errc = acceptor.listen_at( szIP, port );
    if( errc )
    {
        printf( "tcp server listen_at failed: %d\n", errc );
        return;
    }

    while( 1 )
    {
        // accept new connection
        TcpSocket connSkt = acceptor.accept( &errc );
        if( !connSkt.valid() )
        {
            printf( "tcp server accept failed: %d\n", errc );
            break;
        }

        // receive request
        int recved = 0;
        auto rst = connSkt.recv( ubuf.get(), kBufsz, recved );
        if( !rst )
        {
            printf( "tcp recv failed: %d\n", rst.errc );
            break;
        }
        else
        {
            SocketAddr cliAddr;
            connSkt.get_peer_addr( &cliAddr );
            ubuf[recved] = 0;
            printf( "tcp recv from %s : %s\n", cliAddr.to_string().c_str(), ubuf.get() );
        }

        if( stricmp( ubuf.get(), "exit" ) == 0 )
            break;

        // echo back
        int sent = 0;
        rst = connSkt.sendall( ubuf.get(), recved, sent );
        if( !rst || sent != recved )
        {
            printf( "tcp send failed: %d\n", rst.errc );
            break;
        }
        
        // close connection
        errc = connSkt.shutdown();
        if( errc )
            printf( "tcp shutdown errc = %d\n", errc );
    }
}

int main( int argc, char** argv )
{
    network_setup();

    if( argc >= 4 && stricmp(argv[1], "-udp") == 0 )
    {
        launch_udp_server(argv[2], argv[3]);
    }
    else if( argc >= 4 && stricmp(argv[1], "-tcp") == 0 )
    {
        launch_tcp_server(argv[2], argv[3]);
    }
    else
    {
        printf( "EchoServer: invalid startup parameters!\n" );
        return -1;
    }

    return 0;
}
