#include "gtest/gtest.h"
#include "IrkSocket.h"
#include "IrkProcess.h"
#include "IrkThread.h"
#include "IrkEnvUtility.h"

using namespace irk;

TEST( Socket, UDP )
{
    std::string exePath = get_proc_dir();
#ifdef _WIN32
    exePath += '\\';
    exePath += "EchoServer.exe";
#else
    exePath += '/';
    exePath += "EchoServer";
#endif

    // launch echo server
    const char* ipAddr = "127.0.0.1";
    const uint16_t port = 28868;
    CmdLine cmdline;
    cmdline.set_exepath( exePath.c_str() );
    cmdline.add_opt( "-udp" );
    cmdline.add_opt( ipAddr );
    cmdline.add_opt( std::to_string(port).c_str() );
    OSProcess echoSrv;
    ASSERT_TRUE( echoSrv.launch( cmdline ) == 0 );
    OSThread::sleep( 200 );

    UdpSocket client;
    int errc = client.open();
    ASSERT_EQ( 0, errc );

    const SocketAddr srvAddr( ipAddr, port );
    SocketAddr peer;
    constexpr int kBufsz = 127;
    char sendBuf[kBufsz+1] = {};
    char recvBuf[kBufsz+1] = {};
    const int loopCnt = 10;
    for( int i = 1; i <= loopCnt; i++ )
    {
        int datalen = snprintf( sendBuf, kBufsz, "test UDP client %d", i );
        int sent = 0;
        auto rst = client.sendto( srvAddr, sendBuf, datalen, sent );
        if( !rst )
        {
            printf( "udp sendto failed: %d\n", rst.errc );
            break;
        }
        EXPECT_EQ( datalen, sent );
        
        memset( recvBuf, 0, kBufsz );
        int recved = 0;
        rst = client.recvfrom( peer, recvBuf, kBufsz, recved );
        if( !rst )
        {
            printf( "udp recvfrom failed: %d\n", rst.errc );
            break;
        }
        else
        {
            recvBuf[recved] = 0;
            EXPECT_EQ( datalen, recved );
            EXPECT_STREQ( sendBuf, recvBuf );
        }

        OSThread::yield();
    }

    // send exit message
    int sent2 = 0;
    client.sendto( srvAddr, "exit", 4, sent2 );
    if( sent2 == 4 )
        echoSrv.wait();
    else
        echoSrv.kill();
}

TEST( Socket, TCP )
{
    std::string exePath = get_proc_dir();
#ifdef _WIN32
    exePath += '\\';
    exePath += "EchoServer.exe";
#else
    exePath += '/';
    exePath += "EchoServer";
#endif

    // launch echo server
    const char* ipAddr = "127.0.0.1";
    const uint16_t port = 28869;
    CmdLine cmdline;
    cmdline.set_exepath( exePath.c_str() );
    cmdline.add_opt( "-tcp" );
    cmdline.add_opt( ipAddr );
    cmdline.add_opt( std::to_string(port).c_str() );
    OSProcess echoSrv;
    ASSERT_TRUE( echoSrv.launch( cmdline ) == 0 );
    OSThread::sleep( 200 );

    constexpr int kBufsz = 127;
    char sendBuf[kBufsz+1] = {};
    char recvBuf[kBufsz+1] = {};
    const int loopCnt = 10;
    for( int i = 1; i <= loopCnt; i++ )
    {
        int datalen = 0;
        if( i == loopCnt )
        {
            strncpy( sendBuf, "exit", kBufsz );
            datalen = 4;
        }
        else
        {
            datalen = snprintf( sendBuf, kBufsz, "test TCP client %d", i );
        }

        TcpSocket skt;
        int errc = skt.connect( ipAddr, port );
        if( errc )
        {
            printf( "tcp client connect failed: %d\n", errc );
            break;
        }

        int sent = 0;
        auto rst = skt.sendall( sendBuf, datalen, sent );
        if( !rst )
        {
            printf( "tcp send failed: %d\n", rst.errc );
            break;
        }
        EXPECT_EQ( datalen, sent );
        
        if( strcmp(sendBuf, "exit") == 0 )
        {
            echoSrv.wait();
            break;
        }
        else
        {
            memset( recvBuf, 0, kBufsz );
            int recved = 0;  
            rst = skt.recv( recvBuf, kBufsz, recved );
            if( !rst )
            {
                printf( "tcp recv failed: %d\n", rst.errc );
                break;
            }
            else
            {
                recvBuf[recved] = 0;
                EXPECT_EQ( datalen, recved );
                EXPECT_STREQ( sendBuf, recvBuf );
            }
            skt.shutdown();
        }
        
        OSThread::yield();
    }
}
