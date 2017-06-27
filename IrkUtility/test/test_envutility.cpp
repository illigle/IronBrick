#include "gtest/gtest.h"
#include "IrkStringUtility.h"
#include "IrkEnvUtility.h"

using namespace irk;

TEST( EnvUtility, EnvUtility )
{
    std::string u8temp;
    std::u16string u16temp;

    std::string exePath = get_proc_path();
    EXPECT_FALSE( exePath.empty() );
    std::string exePath8 = get_proc_path_u8();
    std::u16string exePath16 = get_proc_path_u16();
    str_utf8to16( exePath8, u16temp );
    EXPECT_EQ( exePath16, u16temp );

    std::string exeDir = get_proc_dir();
    EXPECT_FALSE( exeDir.empty() );
    std::string exeDir8 = get_proc_dir_u8();
    std::u16string exeDir16 = get_proc_dir_u16();
    str_utf8to16( exeDir8, u16temp );
    EXPECT_EQ( exeDir16, u16temp );

    std::string exeName = get_proc_name();
    EXPECT_FALSE( exeDir.empty() );
    std::string exeName8 = get_proc_name_u8();
    std::u16string exeName16 = get_proc_name_u16();
    str_utf8to16( exeName8, u16temp );
    EXPECT_EQ( exeName16, u16temp );

    std::string homeDir = get_home_dir();
    EXPECT_FALSE( homeDir.empty() );
    std::string homeDir8 = get_home_dir_u8();
    std::u16string homeDir16 = get_home_dir_u16();
    str_utf16to8( homeDir16, u8temp );
    EXPECT_EQ( homeDir8, u8temp );

    std::string currDir = get_curr_dir();
    EXPECT_FALSE( currDir.empty() );
    std::string currDir8 = get_curr_dir_u8();
    std::u16string currDir16 = get_curr_dir_u16();
    str_utf16to8( currDir16, u8temp );
    EXPECT_EQ( currDir8, u8temp );

    std::string binPath = get_envar( "PATH" );
    EXPECT_FALSE( binPath.empty() );

    std::string env1 = homeDir + "123";
    set_envar( "MyEnv1", env1.c_str() );
    EXPECT_EQ( env1, get_envar("MyEnv1") );
    std::u16string env2 = currDir16 + u"XYZ";
    set_envar( u"MyEnv2", env2.c_str() );
    EXPECT_EQ( env2, get_envar(u"MyEnv2") );

    EXPECT_TRUE( remove_envar(u"MyEnv1") );
    EXPECT_TRUE( remove_envar("MyEnv2") );
}
