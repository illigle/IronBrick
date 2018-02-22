#include <array>
#include <string>
#include <vector>
#include <scoped_allocator>
#include "gtest/gtest.h"
#include "IrkAllocator.h"

using namespace irk;

namespace {
struct Counter
{
    explicit Counter(int& x) : m_refx(x) { x++; }
    ~Counter() { m_refx++; }
private:
    int& m_refx;
};
}

TEST(Allocator, Arena)
{
    std::array<char, 256> buf;
    MemArena arena(buf.data(), buf.size());

    using String_t = std::basic_string<char, std::char_traits<char>, ArenaAllocator<char>>;
    using Vect_t = std::vector<String_t, std::scoped_allocator_adaptor<ArenaAllocator<String_t>>>;
    const std::string abc = "abcdefghijklmnopqrstuvwxyz";
    const std::string abc123 = abc + "123";

    if (1)
    {
        String_t s1(abc.c_str(), ArenaAllocator<char>(&arena));
        s1 += "123";
        EXPECT_STREQ(abc123.c_str(), s1.c_str());
        s1.insert(0, "+");
        EXPECT_EQ('+', s1[0]);
        EXPECT_STREQ(abc123.c_str(), s1.c_str() + 1);
    }

    if (1)
    {
        Vect_t vec{ArenaAllocator<String_t>(&arena)};
        vec.emplace_back(abc.c_str());
        vec.emplace_back(abc.c_str());
        vec.emplace_back("xyz");
        vec.pop_back();
        for (const auto& s : vec)
        {
            EXPECT_STREQ(abc.c_str(), s.c_str());
        }
    }
}

TEST(Allocator, Chunk)
{
    MemChunks chunk(4096);

    using String_t = std::basic_string<char, std::char_traits<char>, ChunkAllocator<char>>;
    using Vect_t = std::vector<String_t, std::scoped_allocator_adaptor<ChunkAllocator<String_t>>>;
    const std::string abc = "abcdefghijklmnopqrstuvwxyz";
    const std::string abc123 = abc + "123";

    if (1)
    {
        String_t s1(abc.c_str(), ChunkAllocator<char>(&chunk));
        s1 += "123";
        EXPECT_STREQ(abc123.c_str(), s1.c_str());
        s1.insert(0, "+");
        EXPECT_EQ('+', s1[0]);
        EXPECT_STREQ(abc123.c_str(), s1.c_str() + 1);
    }

    if (1)
    {
        Vect_t vec{ChunkAllocator<String_t>(&chunk)};
        vec.emplace_back("xyz");
        vec.emplace_back(abc.c_str());
        vec.emplace_back(abc.c_str());
        vec.erase(vec.begin());
        for (const auto& s : vec)
        {
            EXPECT_STREQ(abc.c_str(), s.c_str());
        }
    }

    chunk.clear();
}

TEST(Allocator, Pool)
{
    MemPool pool(4096);

    using String_t = std::basic_string<char, std::char_traits<char>, PoolAllocator<char>>;
    using Vect_t = std::vector<String_t, std::scoped_allocator_adaptor<PoolAllocator<String_t>>>;
    const std::string abc = "abcdefghijklmnopqrstuvwxyz";
    const std::string abc123 = abc + "123";

    if (1)
    {
        String_t s1(abc.c_str(), PoolAllocator<char>(&pool));
        s1 += "123";
        EXPECT_STREQ(abc123.c_str(), s1.c_str());
        s1.insert(0, "+");
        EXPECT_EQ('+', s1[0]);
        EXPECT_STREQ(abc123.c_str(), s1.c_str() + 1);
    }

    if (1)
    {
        Vect_t vec{PoolAllocator<String_t>(&pool)};
        vec.emplace_back("xyz");
        vec.emplace_back(abc.c_str());
        vec.emplace_back(abc.c_str());
        vec.erase(vec.begin());
        for (const auto& s : vec)
        {
            EXPECT_STREQ(abc.c_str(), s.c_str());
        }
    }

    int cc = 10;
    auto p1 = std::allocate_shared<Counter>(PoolAllocator<String_t>(&pool), cc);
    EXPECT_EQ(11, cc);
    p1.reset();
    EXPECT_EQ(12, cc);
}
