#include "gtest/gtest.h"
#include "IrkXML.h"
#include "IrkBase64.h"
#include "IrkCFile.h"
#include "IrkDirWalker.h"
#include "IrkEnvUtility.h"

using namespace irk;
using std::string;

namespace {

class FsPath : public std::string
{
#ifdef _WIN32
    static const char kSlash = '\\';
#else
    static const char kSlash = '/';
#endif

public:
    FsPath() = default;
    FsPath( const std::string& str ) : std::string(str) {}
    FsPath( std::string&& str ) : std::string(std::move(str)) {}

    void operator /=( const char* leaf )
    {
        *this += kSlash;
        *this += leaf;
    }
    void operator /=( const std::string& leaf )
    {
        *this += kSlash;
        *this += leaf;
    }
    std::string filename() const
    {
        size_t pos = this->rfind( kSlash );
        if( pos != std::string::npos )
            return this->substr( pos + 1 );
        return std::string{};
    }
};

}

static FsPath get_testdir()
{
    FsPath testDir( get_home_dir() );
    testDir /= "TestData";
    testDir /= "xml";
    return testDir;
}

static FsPath get_outdir()
{
    FsPath outputDir( get_home_dir() );
    outputDir /= "TestData";
    outputDir /= "xml";
    outputDir /= "_output";
    if( !irk_fexist( outputDir.c_str() ) )
        irk_mkdir( outputDir.c_str() );
    return outputDir;
}

TEST( XML, Create )
{
    XmlDoc doc;
    EXPECT_FALSE( (bool)doc );
    EXPECT_EQ( nullptr, doc.root() );

    // create xml
    const char* rootTag = "Person";
    XmlElem* root = doc.create_xml( rootTag );
    EXPECT_TRUE( (bool)doc );
    int tagLen = 0;
    const char* tag = root->tag( tagLen );
    EXPECT_EQ( strlen(rootTag), tagLen );
    EXPECT_STREQ( rootTag, tag );

    // add attribute
    root->add_attr( "name", "wei" );
    root->add_attr( std::string("age"), 30 );
    XmlAttr* attr = root->add_attr( "gender" );
    attr->set_value( "male+extra", 4 );
    EXPECT_STREQ( "male", attr->value() );

    // find attribute
    attr = root->find_attr( "age1" );
    EXPECT_EQ( nullptr, attr );
    attr = root->find_attr( "age" );
    EXPECT_NE( nullptr, attr );
    EXPECT_EQ( 30, attr->as_int() );

    // add child
    root->add_child( "home", "shenzhen" );
    root->add_child( "sacred", 100.101 );
    root->add_child( "shadow" );
    root->add_child( "adult", true );
    XmlElem* company = root->add_child( "company" );
    company->set_comment( "The company employed" );
    company->add_child( "name", "google" );
    company->add_child( "number", 1000 );

    // find element
    XmlElem* elem = root->find_child( "home" );
    EXPECT_NE( nullptr, elem );
    EXPECT_STREQ( "shenzhen", elem->content() );

    XmlElem* loc = company->add_child( "location" );
    loc->add_child( "site", u8"北京&<?>" );
    loc->add_child( "site", u8"深圳&<#>" );
    loc->add_attr( "count", 2 );

    // attributes traversal
    for( auto& a : root->attr_range() )
    {
        printf( "%s=\"%s\"\n", a.name(), a.value() );
    }

    // child elements traversal
    for( auto& e : *root )
    {
        printf( "<%s>%s<%s>\n", e.tag(), e.content(), e.tag() );
    }

    FsPath outfile1( get_outdir() );
    FsPath outfile2( outfile1 );
    outfile1 /= "test1.xml";
    outfile2 /= "test2.xml";
    doc.dump_file( outfile1.c_str() );
    doc.pretty_dump_file( outfile2.c_str() );
}

static void parse_xml_file( const FsPath& srcfile )
{
    XmlDoc doc;
    doc.parse_file( srcfile.c_str() );
    if( !doc )
    {
        // only fail*.xml should failed
        string filename = srcfile.filename();
        if( filename.find("fail") == string::npos )
        {
            printf( "parse xml %s failed\n", filename.c_str() );
            EXPECT_TRUE( false );
        }
        return;
    }

    XmlElem* root = doc.root();
    root->add_child( "IronBrick", 2016 );

    FsPath outfile( get_outdir() );
    outfile /= srcfile.filename();
    doc.pretty_dump_file( outfile.c_str() );
}

TEST( XML, Parse )
{
    FsPath filePath;
    std::string dir = get_testdir();

    for( const DirEntry* entry : DirWalker(dir.c_str()) )
    {
        if( ::strcmp(entry->extension(), ".xml") == 0  )
        {
            filePath.assign( dir );
            filePath /= entry->name;
            parse_xml_file( filePath );
        }
    }
}
