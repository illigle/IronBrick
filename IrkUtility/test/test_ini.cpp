#include "gtest/gtest.h"
#include "IrkIniFile.h"
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
    testDir /= "ini";
    return testDir;
}

static FsPath get_outdir()
{
    FsPath outputDir( get_home_dir() );
    outputDir /= "TestData";
    outputDir /= "ini";
    outputDir /= "_output";
    if( !irk_fexist( outputDir.c_str() ) )
        irk_mkdir( outputDir.c_str() );
    return outputDir;
}

TEST( INI, Writer )
{
    IniWriter writer;

    writer.add_property( "Info", "address", "shenzhen" );
    writer.add_property( std::string("Info"), std::string("age"), int_to_string(35) );
    writer.add_property( "Info", "title", "engineer" );
    writer.add_property( "Info", "gender", "male" );
    writer.add_property( "Info", "nothing", "xxxx-yyyy" );
    EXPECT_EQ( 5, writer[0].property_count() );
    writer.remove_property( "Info", "nothing" );
    EXPECT_EQ( 4, writer[0].property_count() );

    writer.add_property( "SecDead", "xxx", "123" );
    EXPECT_EQ( 2, writer.section_count() );
    writer.remove_section( "SecDead" );
    EXPECT_EQ( 1, writer.section_count() );

    writer.add_property( "Extra", "brightness", "100" );
    writer.add_property( "Extra", "sound", "90" );

    FsPath outfile1( get_outdir() );
    outfile1 /= "test1.ini";
    writer.dump_file( outfile1.c_str() );

    std::string outstr;
    writer.dump_text( outstr );
    FsPath outfile2( get_outdir() );
    outfile2 /= "test2.ini";
    CFile outfp( outfile2.c_str(), "w" );
    if( outfp )
        fprintf( outfp, "%s", outstr.c_str() );
}

TEST( INI, inline_comment_escaped )
{
    IniParser parser( true, true );
    IniWriter writer1( true );
    IniWriter writer2;

    FsPath srcfile( get_testdir() );
    srcfile /= "escaped_inline_comment.ini";

    bool success = parser.parse_file( srcfile.c_str() );
    EXPECT_TRUE( success );

    for( unsigned i = 0; i < parser.section_count(); i++ )
    {
        const auto& section = parser[i];
        const char* secName = section.section_name();
        for( unsigned k = 0; k < section.property_count(); k++ )
        {
            const char* propName = section[k].name;
            const char* propValue = section[k].value;
            EXPECT_EQ( propValue, parser.get_property_value(secName, propName) );
            writer1.add_property( secName, propName, propValue );
            writer2.add_property( secName, propName, propValue );
        }
    }

    FsPath outfile1( get_outdir() );
    outfile1 /= "1_escaped_inline_comment.ini";
    writer1.dump_file( outfile1.c_str() );

    FsPath outfile2( get_outdir() );
    outfile2 /= "2_escaped_inline_comment.ini";
    writer2.dump_file( outfile2.c_str() );
}

static void parse_ini_file( const FsPath& srcfile )
{
    IniParser parser;
    IniWriter writer;

    bool success = parser.parse_file( srcfile.c_str() );
    EXPECT_TRUE( success );

    for( unsigned i = 0; i < parser.section_count(); i++ )
    {
        const auto& section = parser[i];
        const char* secName = section.section_name();
        for( unsigned k = 0; k < section.property_count(); k++ )
        {
            const char* propName = section[k].name;
            const char* propValue = section[k].value;
            EXPECT_EQ( propValue, parser.get_property_value(secName, propName) );
            writer.add_property( secName, propName, propValue );
        }
    }

    writer.add_property( "zzGenerator", "Tool", "IronBrick" );
    writer.add_property( "zzGenerator", "Author", "illigle" );
    FsPath outfile( get_outdir() );
    outfile /= srcfile.filename();
    writer.dump_file( outfile.c_str() );
}

TEST( INI, Parse )
{
    FsPath filePath;
    std::string dir = get_testdir();
    
    for( const DirEntry* entry : DirWalker(dir.c_str()) )
    {
        if( ::strcmp(entry->extension(), ".ini") == 0  )
        {
            filePath.assign( dir );
            filePath /= entry->name;
            parse_ini_file( filePath );
        }
    }
}

