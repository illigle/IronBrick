#include <string.h>
#include <array>
#include <utility>
#include "gtest/gtest.h"
#include "IrkCFile.h"
#include "IrkFileWalker.h"
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

    if (!irk_fexist(workDir.c_str()))
        irk_mkdir(workDir.c_str());
    ::chdir(workDir.c_str());
    EXPECT_TRUE(irk_fexist(workDir.c_str()));
}

TEST(CFile, stdc)
{
    prepare_working_dir();

    // open
    const char* filename = "stdc_test.txt";
    CFile file;
    EXPECT_TRUE(!file);
    file.open(filename, "w+");
    ASSERT_TRUE(file);
    EXPECT_EQ(0, file.file_size());
    EXPECT_GT(file.create_time(), -1);
    EXPECT_GT(file.modify_time(), -1);

    // read write
    std::array<char, 16> src, dst;
    src.fill('a');
    src.back() = 0;
    ::fputs(src.data(), file);
    ::rewind(file);
    ::fgets(dst.data(), (int)dst.size(), file);
    EXPECT_STREQ(src.data(), dst.data());

    FileStat st = {};
    file.file_stat(&st);
    EXPECT_EQ(file.file_size(), st.st_size);
    EXPECT_EQ(file.modify_time(), st.st_mtime);

    // close
    file.close();
    EXPECT_EQ(nullptr, file.get());
    EXPECT_EQ(-1, file.file_size());
    EXPECT_EQ(-1, file.create_time());
    EXPECT_EQ(-1, file.modify_time());
    EXPECT_GT(irk_fsize(filename), 0);
}

TEST(CFile, posix)
{
    prepare_working_dir();

    // open
    const char* filename = "posix_test.bin";
    PosixFile file;
    EXPECT_TRUE(!file);
    file.open(filename, O_RDWR | O_CREAT | O_TRUNC);
    ASSERT_TRUE(file);
    EXPECT_EQ(0, file.file_size());
    EXPECT_GT(file.create_time(), -1);
    EXPECT_GT(file.modify_time(), -1);

    // read write
    std::array<uint8_t, 16> src, dst;
    src.fill(0x26);
    ::write(file, src.data(), (int)src.size());
    irk_fseek(file, 0, 0);
    ::read(file, dst.data(), (int)dst.size());
    EXPECT_EQ(src, dst);

    FileStat st = {};
    file.file_stat(&st);
    EXPECT_EQ(file.file_size(), st.st_size);
    EXPECT_EQ(file.modify_time(), st.st_mtime);

    // close
    file.close();
    EXPECT_EQ(-1, file.get());
    EXPECT_EQ(-1, file.file_size());
    EXPECT_EQ(-1, file.create_time());
    EXPECT_EQ(-1, file.modify_time());
    EXPECT_GE(irk_fsize(filename), (int64_t)src.size());
}

TEST(CFile, freefunc)
{
    prepare_working_dir();

    FileStat st = {};

    // open, close
    auto file1 = irk_fopen("1.txt", "w");
    auto file2 = irk_fopen("2.txt", O_RDWR | O_CREAT | O_TRUNC);
    ASSERT_TRUE(file1);
    ASSERT_TRUE(file2);
    EXPECT_EQ(0, file1.file_size());
    EXPECT_EQ(0, file2.file_size());
    irk_fstat("1.txt", &st);
    EXPECT_EQ(st.st_ctime, irk_fctime("1.txt"));
    irk_fstat(file2.get(), &st);
    EXPECT_EQ(st.st_ctime, irk_fctime(file2.get()));
    file1.close();
    file2.close();

    EXPECT_TRUE(irk_fexist("1.txt"));
    EXPECT_TRUE(irk_fexist("2.txt"));
    EXPECT_GT(irk_fctime("1.txt"), 0);
    EXPECT_GT(irk_fmtime("2.txt"), 0);

    // rename, copy
    irk_rename("1.txt", "100.txt");
    EXPECT_FALSE(irk_fexist("1.txt"));
    EXPECT_TRUE(irk_fexist("100.txt"));
    irk_fcopy("2.txt", "200.txt");
    EXPECT_TRUE(irk_fexist("2.txt"));
    EXPECT_TRUE(irk_fexist("200.txt"));
    file1 = irk_fopen("1.txt", "w");
    EXPECT_FALSE(irk_rename("1.txt", "100.txt", true));
    EXPECT_FALSE(irk_fcopy("1.txt", "200.txt", true));

    // remove file
    irk_unlink("200.txt");
    EXPECT_FALSE(irk_fexist("200.txt"));

    // create, delete dir
    irk_mkdir("abc");
    EXPECT_TRUE(irk_fexist("abc"));
    irk_rmdir("abc");
    EXPECT_FALSE(irk_fexist("abc"));
}

TEST(CFile, filewalker)
{
    prepare_working_dir();

    std::array<std::string, 12> lines;
    for (size_t i = 0; i < lines.size(); i++)
        lines[i].assign(1LL << i, '#');

    const char* filename = "walker.txt";
    CFile file(filename, "w");
    ASSERT_TRUE(file);
    for (const auto& line : lines)
        fprintf(file, "%s\n", line.c_str());
    file.close();

    FileWalker walker(filename);
    ASSERT_TRUE(walker.is_opened());

    // walk through lines
    const char* str = nullptr;
    size_t lineCnt = 0;
    while ((str = walker.next_line()) != nullptr)
    {
        EXPECT_STREQ(str, lines[lineCnt].c_str());
        lineCnt++;
    }
    EXPECT_EQ(lines.size(), lineCnt);

    // iterate lines
    FileWalker walker2(std::move(walker));    // move construction
    EXPECT_FALSE(walker.is_opened());
    walker2.rewind();
    lineCnt = 0;
    for (const char* line : walker2)
    {
        EXPECT_STREQ(line, lines[lineCnt].c_str());
        lineCnt++;
    }
    EXPECT_EQ(lines.size(), lineCnt);
}
