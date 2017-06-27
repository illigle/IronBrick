#include "gtest/gtest.h"
#include "IrkLogger.h"
#include "IrkCFile.h"
#include "IrkOnExit.h"
#include "IrkEnvUtility.h"

using namespace irk;

static void prepare_working_dir()
{
    std::string workDir = get_home_dir();
#ifdef _WIN32
    workDir += "\\TestData";
#else
    workDir += "/TestData";
#endif

    if( !irk_fexist( workDir.c_str() ) )
        irk_mkdir( workDir.c_str() );
    ::chdir( workDir.c_str() );
    EXPECT_TRUE( irk_fexist( workDir.c_str() ) );
}

static std::string get_working_dir()
{
    std::string workDir = get_home_dir();
#ifdef _WIN32
    workDir += "\\TestData";
#else
    workDir += "/TestData";
#endif
    return workDir;
}

#ifdef _WIN32
static std::wstring get_working_dir_w()
{
    std::wstring workDir = get_home_dir_ws();
    workDir += L"\\TestData";
    return workDir;
}
#endif

TEST( Logger, Logger )
{
    prepare_working_dir();
    const int errc = -1000;

    {
        Logger* plog = new DumbLogger;
        ON_EXIT( delete plog; );
        irk_blame( plog, "error! %d\n", errc );
        irk_warn( plog, "warning: %d\n", errc );
        irk_trace( plog, "info> %d\n", errc );
        irk_blame( plog, L"error! %d\n", errc );
        irk_warn( plog, L"warning: %d\n", errc );
        irk_trace( plog, L"info> %d\n", errc );
    }
    {
        Logger* plog = new StdLogger;
        ON_EXIT( delete plog; );
        irk_blame( plog, "error! %d\n", errc );
        irk_warn( plog, "warning: %d\n", errc );
        irk_trace( plog, "info> %d\n", errc );
    }
    {
        Logger* plog = new ColorLogger;
        ON_EXIT( delete plog; );
        irk_blame( plog, "error! %d\n", errc );
        irk_warn( plog, "warning: %d\n", errc );
        irk_trace( plog, "info> %d\n", errc );
    }
    {
        FileLogger* plog = new FileLogger;
        plog->open( "file.log" );
        ON_EXIT( delete plog; );
        irk_blame( plog, "error! %d\n", errc );
        irk_warn( plog, "warning: %d\n", errc );
        irk_trace( plog, "info> %d\n", errc );
    }
    {
        MonthlyLogger* plog = new MonthlyLogger;
        plog->open( get_working_dir().c_str(), "month" );
        ON_EXIT( delete plog; );
        irk_blame( plog, "error! %d\n", errc );
        irk_warn( plog, "warning: %d\n", errc );
        irk_trace( plog, "info> %d\n", errc );
    }
#ifdef _WIN32
    {
        MonthlyLogger* plog = new MonthlyLogger;
        plog->open( get_working_dir_w().c_str(), L"utf8-month" );
        ON_EXIT( delete plog; );
        irk_blame( plog, L"错误! %d\n", errc );
        irk_warn( plog, L"警告: %d\n", errc );
        irk_trace( plog, L"信息> %d\n", errc );
    }
#endif
}


