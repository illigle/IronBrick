#include "gtest/gtest.h"
#include "IrkCmdLine.h"

using namespace irk;

TEST(Cmdline, Cmdline)
{
    CmdLine cmdline;
    std::string tmp = cmdline.to_string();
    EXPECT_TRUE(tmp.empty());

    const char* szCmd = "self.exe -x -y --debug --input=123.txt out.txt";
    cmdline.parse(szCmd);

    EXPECT_STREQ("self.exe", cmdline.exepath().c_str());
    EXPECT_EQ(6, cmdline.arg_count());
    EXPECT_EQ(5, cmdline.opt_count());
    EXPECT_EQ(cmdline[0], cmdline.exepath());
    EXPECT_STREQ("-x", cmdline[1].c_str());
    EXPECT_STREQ("--debug", cmdline[3].c_str());
    EXPECT_EQ(nullptr, cmdline.get_optvalue("-x"));
    EXPECT_EQ(nullptr, cmdline.get_optvalue("-name"));
    EXPECT_STREQ("123.txt", cmdline.get_optvalue("--input"));

    std::string cmdstr = cmdline.to_string();
    EXPECT_STREQ(szCmd, cmdstr.c_str());

    cmdline.reset();
    cmdline.set_exepath("/home/me/worker.bin");
    cmdline.add_opt("-g");
    cmdline.add_opt("-user", "lucy");
    cmdline.add_opt("-pwd", "123456");
    cmdline.set_opt("-root", "/home/lucy");
    cmdline.set_opt("-pwd", "XYZ123");
    cmdstr = cmdline.to_string();
    EXPECT_STREQ("/home/me/worker.bin -g -user=lucy -pwd=XYZ123 -root=/home/lucy", cmdstr.c_str());
    EXPECT_STREQ("lucy", cmdline.get_optvalue("-user"));
    EXPECT_STREQ("/home/lucy", cmdline.get_optvalue("-root"));
}
