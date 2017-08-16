#include "gtest/gtest.h"
#include "IrkURI.h"

using namespace irk;

TEST( URI, URI )
{
    Uri uri;
    uri.set_scheme( "HTTP" );
    uri.set_host( u8"test-bin-@.中国" );
    uri.set_port( 8090 );
    uri.set_userinfo( u8"wei:123" );
    uri.set_path( u8"/my 测试.txt" );
    uri.set_query( "ask=123(hi!)&req=5" );
    uri.set_fragment( "1" );
    
    EXPECT_STRCASEEQ( "http", uri.scheme().c_str() );
    
    std::string server = uri.escaped_server();
    EXPECT_STREQ( "wei:123@test-bin-%40.%E4%B8%AD%E5%9B%BD:8090", server.c_str() );

    std::string resource = uri.escaped_resource();
    EXPECT_STREQ( "/my%20%E6%B5%8B%E8%AF%95.txt?ask=123(hi!)&req=5#1", resource.c_str() );

    std::string euri = uri.escaped_uri();
    EXPECT_TRUE( is_valid_escaped_uri( euri.c_str() ) );
    std::string euri2 = "http://";
    euri2 += server;
    euri2 += resource;
    EXPECT_EQ( euri2, euri );

    EXPECT_FALSE( is_valid_escaped_uri( "c:\\windows" ) );

    Uri uri2;
    uri2.set_escaped_uri( euri.c_str() );
    EXPECT_EQ( uri, uri2 );

    const char* normalUri = R"(https://help.aliyun.com/document_detail/32131.html?q123#f456)";
    uri2.set_plain_uri( normalUri );
    EXPECT_STRCASEEQ( "https", uri2.scheme().c_str() );
    EXPECT_STRCASEEQ( "help.aliyun.com", uri2.escaped_server().c_str() );
    EXPECT_STREQ( "/document_detail/32131.html?q123#f456", uri2.escaped_resource().c_str() );
    EXPECT_STREQ( normalUri, uri2.escaped_uri().c_str() );
}
