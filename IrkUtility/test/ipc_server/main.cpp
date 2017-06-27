#include "IrkStringUtility.h"
#include "IrkSyncUtility.h"
#include "IrkCmdLine.h"
#include "IrkIPC.h"
#include <errno.h>

using namespace irk;

#define ssizeof(x) (int)sizeof(x)

static void to_upper( char* str )
{
    while( *str )
    {
        if( *str >= 'a' && *str <= 'z' )
            *str -= 32;
        str++;
    }
}
static void to_upper( char* str, int len )
{
    for( int i = 0; i < len; i++ )
    {
        if( str[i] >= 'a' && str[i] <= 'z' )
            str[i] -= 32;
    }
}

static void launch_freight_server( const char* name, const char* semaName )
{
    IpcFreightServer server;

    IpcResult rst = server.create( name, 4096 );
    if( !rst )
    {
        fprintf( stderr, "!!! create freight server %s failed: %d\n", name, rst.errc );
        return;
    }

    // notify client I'm is ready
    Semaphore ready;
    rst = ready.open( semaName );
    if( !rst )
    {
        fprintf( stderr, "!!! open semaphore %s failed\n", semaName );
        return;
    }
    ready.post();

    IpcCargo cargo;
    int cnt = 0; 
    for( cnt = 0; cnt < 1000; cnt++ )
    {
        // recv request
        rst = server.recv( cargo );
        if( !rst )
        {
            fprintf( stderr, "!!! freight recv req failed %d\n", rst.errc );
            break;
        }

        // check exit
        if( stricmp( (const char*)cargo.data, "exit" ) == 0 )
        {
            printf( "==== freight server got exit message ====\n" );
            server.discard( cargo );
            break;
        }

        to_upper( (char*)cargo.data );

        // send ack
        rst = server.send( cargo );
        if( !rst )
        {
            fprintf( stderr, "!!! freight send ack failed %d\n", rst.errc );
            break;
        }
    }

    printf( "==== freight server got %d requests ====\n", cnt );
}

static void launch_mq_server( const char* name, const char* semaName )
{
    struct IpcMessage
    {
        int id;
        int seq;
        int param;
        char data[256];
    };

    IpcMessage msg;
    IpcMqServer server;

    IpcResult rst = server.create( name, ssizeof(msg) );
    if( !rst )
    {
        fprintf( stderr, "!!! create mq %s failed: %d\n", name, rst.errc );
        return;
    }

    // notify client I'm is ready
    Semaphore ready;
    rst = ready.open( semaName );
    if( !rst )
    {
        fprintf( stderr, "!!! open semaphore %s failed\n", semaName );
        return;
    }
    ready.post();

    // listen for client connection
    rst = server.wait_connected( 2000 );
    if( !rst )
    {
        fprintf( stderr, "!!! mq wait_connected failed %d\n", rst.errc );
        return;
    }

    int recvSize = 0;
    int cnt = 0;
    for( cnt = 0; cnt < 1000; cnt++ )
    {
        // recv request
        memset( &msg, 0, sizeof(msg) );
        rst = server.recv( &msg, ssizeof(msg), recvSize );
        if( !rst )
        {
            fprintf( stderr, "!!! mq recv req failed %d\n", rst.errc );
            break;
        }

        // check exit
        if( stricmp( msg.data, "exit" ) == 0 )
        {
            printf( "==== mq server got exit message: %d ====\n", msg.seq );
            break;
        }

        to_upper( msg.data );

        // send ack
        if( msg.id == 0 )
        {
            msg.seq *= 10;
            msg.param = (int)strlen( msg.data );
            
            rst = server.send( &msg, ssizeof(msg) );
            if( !rst )
            {
                fprintf( stderr, "!!! mq send failed ack %d\n", rst.errc );
                break;
            }
        }
        else    // only send string back
        {
            rst = server.send( msg.data, (int)strlen(msg.data) + 1 );
            if( !rst )
            {
                fprintf( stderr, "!!! mq send ack failed %d\n", rst.errc );
                break;
            }
        }
    }

    printf( "==== mq server got %d requests ====\n", cnt );
}

static void launch_pipe_server( const char* name, const char* semaName )
{
#ifdef _WIN32
#define ESHUTDOWN 109
#endif

    IpcPipeServer server;
    
    IpcResult rst = server.create( name, 4096 );
    if( !rst )
    {
        fprintf( stderr, "!!! create pipe %s failed: %d\n", name, rst.errc );
        return;
    }

    // notify client I'm is ready
    Semaphore ready;
    rst = ready.open( semaName );
    if( !rst )
    {
        fprintf( stderr, "!!! open semaphore %s failed\n", semaName );
        return;
    }
    ready.post();

    // listen for client connection
    rst = server.wait_connected( 2000 );
    if( !rst )
    {
        fprintf( stderr, "!!! pipe wait_connected failed %d\n", rst.errc );
        return;
    }

    char buff[512] = {};
    while( 1 )
    {
        // recv
        int recvCnt = -1;
        rst = server.recv( buff, ssizeof(buff), recvCnt );
        if( !rst )
        {
            if( rst.errc == ESHUTDOWN )
            {
                printf( "=== pipe client shutdown ===\n" );
            }
            else
            {
                fprintf( stderr, "!!! pipe recv req failed %d\n", rst.errc );
            }
            break;
        }

        // to upper case
        to_upper( buff, recvCnt );

        rst = server.send( buff, recvCnt );
        if( !rst )
        {
            fprintf( stderr, "!!! pipe send ack failed %d\n", rst.errc );
            break;
        }
    }
    
    printf( "=== pipe server exit ===\n" );
}

int main( int argc, char** argv )
{
    CmdLine cmdline;
    cmdline.parse( argc, argv );

    const char* semaName = cmdline.get_optvalue( "-sema" );
    if( !semaName )
    {
        fprintf( stderr, "!!! Invalid command: %s\n", cmdline.to_string().c_str() );
        return -1;
    }

    const char* freightName = cmdline.get_optvalue( "-freight" );
    if( freightName )
    {
        launch_freight_server( freightName, semaName );
        return 0;
    }

    const char* mqName = cmdline.get_optvalue( "-mq" );
    if( mqName )
    {
        launch_mq_server( mqName, semaName );
        return 0;
    }

    const char* pipeName = cmdline.get_optvalue( "-pipe" );
    if( pipeName )
    {
        launch_pipe_server( pipeName, semaName );
        return 0;
    }

    fprintf( stderr, "!!! Invalid command: %s\n", cmdline.to_string().c_str() );
    return -1;
}
