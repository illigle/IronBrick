#include "gtest/gtest.h"
#include "IrkCiString.h"
#include <map>
#include <unordered_map>

using namespace std;
using namespace irk;

TEST(CiString, ci_string)
{
    // comparision
    {
        ci_string istr1("AbcXyz123");
        ci_string istr2("abcxyz123");
        EXPECT_STREQ("AbcXyz123", istr1.c_str());
        EXPECT_STREQ("abcxyz123", istr2.c_str());
        EXPECT_EQ(istr1, istr2);
        EXPECT_STRNE(istr1.c_str(), istr2.c_str());
        EXPECT_STRNE("ABCXYZ123", istr2.c_str());
        EXPECT_STRCASEEQ("ABCXYZ123", istr2.c_str());
        EXPECT_TRUE(istr2.compare("ABCXYZ123") == 0);

        // comparision between std::string
        string str1("ABCXYZ123");
        EXPECT_EQ(istr1, str1);
        EXPECT_STRNE(istr1.c_str(), str1.c_str());
        str1 = istr1;
        EXPECT_EQ(istr1, str1);
        EXPECT_STREQ(istr1.c_str(), str1.c_str());
        istr1 = "abcxyz";
        str1 = "ABCXYZ123";
        EXPECT_LT(istr1, str1);
        istr1 = str1;
        istr1 += "[]";
        istr1.append(4, '#');
        str_tolower(str1);
        EXPECT_GT(istr1, str1);
    }

    // common method
    {
        // std method
        ci_string istr1("ABCXYZ123");
        EXPECT_EQ(0, istr1.find("abc"));
        EXPECT_EQ(3, istr1.find_first_of("xyz"));
        istr1.pop_back();
        istr1.push_back('2');
        EXPECT_EQ(0, istr1.compare("abcxyz122"));
        EXPECT_STRCASEEQ("xyz", istr1.substr(3, 3).c_str());
        istr1.resize(3);
        istr1.insert(0, "def");
        EXPECT_STREQ("defABC", istr1.c_str());
        EXPECT_STRCASEEQ("defabc", istr1.c_str());
        string str1("中国");
        istr1 = std::move(str1);
        istr1 += "深圳";
        EXPECT_STREQ("中国深圳", istr1.c_str());
    }

    // case-insensitive hash
    {
        ci_string istr1(8, 'm');
        ci_string istr2(8, 'M');
        EXPECT_EQ(8, istr1.length());
        EXPECT_EQ(8, istr2.length());
        auto h3 = ci_hash<ci_string>()(istr1);
        auto h4 = ci_hash<ci_string>()(istr2);
        EXPECT_EQ(h3, h4);
        EXPECT_EQ(h3, std::hash<ci_string>()(istr1));
        EXPECT_EQ(h3, std::hash<ci_string>()(istr2));
        string str1(8, 'm');
        string str2(8, 'M');
        EXPECT_EQ(h3, ci_hash<string>()(str1));
        EXPECT_EQ(h3, ci_hash<string>()(str2));
        EXPECT_NE(std::hash<string>()(str1), std::hash<string>()(str2));
        // equal_to
        EXPECT_TRUE(ci_equal_to<ci_string>()(istr1, istr2));
        EXPECT_TRUE(std::equal_to<ci_string>()(istr1, istr2));
        EXPECT_TRUE(ci_equal_to<string>()(str1, str2));
        EXPECT_FALSE(std::equal_to<string>()(str1, str2));
    }

    // case-insensitive string map
    {
        std::map<string, int, ci_less<string>> cimap;
        cimap.emplace("abc", 1);
        cimap.emplace("XYZ", 2);
        EXPECT_EQ(1, cimap.count("ABC"));
        EXPECT_EQ(1, cimap.count("xyz"));
        cimap["ABC"] = -1;
        cimap["xyz"] = -2;
        EXPECT_EQ(-1, cimap["abc"]);
        EXPECT_EQ(-2, cimap["XYZ"]);
    }
    {
        std::map<ci_string, int> cimap;
        cimap.emplace("abc", 1);
        cimap.emplace("XYZ", 2);
        EXPECT_EQ(1, cimap.count("ABC"));
        EXPECT_EQ(1, cimap.count("xyz"));
        cimap["ABC"] = -1;
        cimap["xyz"] = -2;
        EXPECT_EQ(-1, cimap["abc"]);
        EXPECT_EQ(-2, cimap["XYZ"]);
    }

    // case-insensitive string hash table
    {
        std::unordered_map<string, int, ci_hash<string>, ci_equal_to<string>> hmap;
        hmap.emplace("abc", 1);
        hmap.emplace("XYZ", 2);
        EXPECT_EQ(1, hmap.count("ABC"));
        EXPECT_EQ(1, hmap.count("xyz"));
        hmap["ABC"] = -1;
        hmap["xyz"] = -2;
        EXPECT_EQ(-1, hmap["abc"]);
        EXPECT_EQ(-2, hmap["XYZ"]);
    }
    {
        std::unordered_map<ci_string, int> hmap;
        hmap.emplace("abc", 1);
        hmap.emplace("XYZ", 2);
        EXPECT_EQ(1, hmap.count("ABC"));
        EXPECT_EQ(1, hmap.count("xyz"));
        hmap["ABC"] = -1;
        hmap["xyz"] = -2;
        EXPECT_EQ(-1, hmap["abc"]);
        EXPECT_EQ(-2, hmap["XYZ"]);
    }
}

TEST(CiString, ci_wstring)
{
    // comparision
    {
        ci_wstring istr1(L"AbcXyz123");
        ci_wstring istr2(L"abcxyz123");
        EXPECT_STREQ(L"AbcXyz123", istr1.c_str());
        EXPECT_STREQ(L"abcxyz123", istr2.c_str());
        EXPECT_EQ(istr1, istr2);
        EXPECT_STRNE(istr1.c_str(), istr2.c_str());
        EXPECT_STRNE(L"ABCXYZ123", istr2.c_str());
        EXPECT_TRUE(istr2.compare(L"ABCXYZ123") == 0);

        // comparision between std::wstring
        wstring str1(L"ABCXYZ123");
        EXPECT_EQ(istr1, str1);
        EXPECT_STRNE(istr1.c_str(), str1.c_str());
        str1 = istr1;
        EXPECT_EQ(istr1, str1);
        EXPECT_STREQ(istr1.c_str(), str1.c_str());
        istr1 = L"abcxyz";
        str1 = L"ABCXYZ123";
        EXPECT_LT(istr1, str1);
        istr1 = str1;
        istr1 += L"[]";
        istr1.append(4, L'#');
        str_tolower(str1);
        EXPECT_GT(istr1, str1);
    }

    // common method
    {
        // std method
        ci_wstring istr1(L"ABCXYZ123");
        EXPECT_EQ(0, istr1.find(L"abc"));
        EXPECT_EQ(3, istr1.find_first_of(L"xyz"));
        istr1.pop_back();
        istr1.push_back(L'2');
        EXPECT_EQ(0, istr1.compare(L"abcxyz122"));
        EXPECT_TRUE(wstring(L"xyz") == istr1.substr(3, 3));
        istr1.resize(3);
        istr1.insert(0, L"def");
        EXPECT_STREQ(L"defABC", istr1.c_str());
        wstring str1(L"中国");
        istr1 = std::move(str1);
        istr1 += L"深圳";
        EXPECT_STREQ(L"中国深圳", istr1.c_str());
    }

    // case-insensitive hash
    {
        ci_wstring istr1(8, L'm');
        ci_wstring istr2(8, L'M');
        EXPECT_EQ(8, istr1.length());
        EXPECT_EQ(8, istr2.length());
        auto h3 = ci_hash<ci_wstring>()(istr1);
        auto h4 = ci_hash<ci_wstring>()(istr2);
        EXPECT_EQ(h3, h4);
        EXPECT_EQ(h3, std::hash<ci_wstring>()(istr1));
        EXPECT_EQ(h3, std::hash<ci_wstring>()(istr2));
        wstring str1(8, L'm');
        wstring str2(8, L'M');
        EXPECT_EQ(h3, ci_hash<wstring>()(str1));
        EXPECT_EQ(h3, ci_hash<wstring>()(str2));
        EXPECT_NE(std::hash<wstring>()(str1), std::hash<wstring>()(str2));
        // equal_to
        EXPECT_TRUE(ci_equal_to<ci_wstring>()(istr1, istr2));
        EXPECT_TRUE(std::equal_to<ci_wstring>()(istr1, istr2));
        EXPECT_TRUE(ci_equal_to<wstring>()(str1, str2));
        EXPECT_FALSE(std::equal_to<wstring>()(str1, str2));
    }

    // case-insensitive wstring map
    {
        std::map<wstring, int, ci_less<wstring>> cimap;
        cimap.emplace(L"abc", 1);
        cimap.emplace(L"XYZ", 2);
        EXPECT_EQ(1, cimap.count(L"ABC"));
        EXPECT_EQ(1, cimap.count(L"xyz"));
        cimap[L"ABC"] = -1;
        cimap[L"xyz"] = -2;
        EXPECT_EQ(-1, cimap[L"abc"]);
        EXPECT_EQ(-2, cimap[L"XYZ"]);
    }
    {
        std::map<ci_wstring, int> cimap;
        cimap.emplace(L"abc", 1);
        cimap.emplace(L"XYZ", 2);
        EXPECT_EQ(1, cimap.count(L"ABC"));
        EXPECT_EQ(1, cimap.count(L"xyz"));
        cimap[L"ABC"] = -1;
        cimap[L"xyz"] = -2;
        EXPECT_EQ(-1, cimap[L"abc"]);
        EXPECT_EQ(-2, cimap[L"XYZ"]);
    }

    // case-insensitive wstring hash table
    {
        std::unordered_map<wstring, int, ci_hash<wstring>, ci_equal_to<wstring>> hmap;
        hmap.emplace(L"abc", 1);
        hmap.emplace(L"XYZ", 2);
        EXPECT_EQ(1, hmap.count(L"ABC"));
        EXPECT_EQ(1, hmap.count(L"xyz"));
        hmap[L"ABC"] = -1;
        hmap[L"xyz"] = -2;
        EXPECT_EQ(-1, hmap[L"abc"]);
        EXPECT_EQ(-2, hmap[L"XYZ"]);
    }
    {
        std::unordered_map<ci_wstring, int> hmap;
        hmap.emplace(L"abc", 1);
        hmap.emplace(L"XYZ", 2);
        EXPECT_EQ(1, hmap.count(L"ABC"));
        EXPECT_EQ(1, hmap.count(L"xyz"));
        hmap[L"ABC"] = -1;
        hmap[L"xyz"] = -2;
        EXPECT_EQ(-1, hmap[L"abc"]);
        EXPECT_EQ(-2, hmap[L"XYZ"]);
    }
}

TEST(CiString, ci_u16string)
{
    // comparision
    {
        ci_u16string istr1(u"AbcXyz123");
        ci_u16string istr2(u"abcxyz123");
        EXPECT_EQ(istr1, istr2);
        EXPECT_TRUE(istr1.compare(u"abcxyz123") == 0);
        EXPECT_TRUE(istr2.compare(u"ABCXYZ123") == 0);

        // comparision between std::u16string
        u16string str1(u"ABCXYZ123");
        EXPECT_EQ(istr1, str1);
        EXPECT_NE(istr1.stdstr(), str1);
        str1 = istr1;
        EXPECT_EQ(istr1, str1);
        EXPECT_EQ(istr1.stdstr(), str1);
        istr1 = u"abcxyz";
        str1 = u"ABCXYZ123";
        EXPECT_LT(istr1, str1);
        istr1 = str1;
        istr1.append(4, u'#');
        str_tolower(str1);
        EXPECT_GT(istr1, str1);
    }

    // common method
    {
        // std method
        ci_u16string istr1(u"ABCXYZ123");
        EXPECT_EQ(0, istr1.find(u"abc"));
        EXPECT_EQ(3, istr1.find_first_of(u"xyz"));
        istr1.pop_back();
        istr1.push_back(L'2');
        EXPECT_EQ(0, istr1.compare(u"abcxyz122"));
        EXPECT_TRUE(u16string(u"xyz") == istr1.substr(3, 3));
        istr1.resize(3);
        istr1.insert(0, u"def");
        EXPECT_EQ(u16string(u"DEFABC"), istr1);
        EXPECT_NE(u16string(u"DEFABC"), istr1.stdstr());
        EXPECT_EQ(u16string(u"defABC"), istr1.stdstr());
        u16string str1(u"中国");
        istr1 = std::move(str1);
        istr1 += u"深圳";
        EXPECT_EQ(u16string(u"中国深圳"), istr1);
    }

    // case-insensitive hash
    {
        ci_u16string istr1(8, L'm');
        ci_u16string istr2(8, L'M');
        EXPECT_EQ(8, istr1.length());
        EXPECT_EQ(8, istr2.length());
        auto h3 = ci_hash<ci_u16string>()(istr1);
        auto h4 = ci_hash<ci_u16string>()(istr2);
        EXPECT_EQ(h3, h4);
        EXPECT_EQ(h3, std::hash<ci_u16string>()(istr1));
        EXPECT_EQ(h3, std::hash<ci_u16string>()(istr2));
        u16string str1(8, L'm');
        u16string str2(8, L'M');
        EXPECT_EQ(h3, ci_hash<u16string>()(str1));
        EXPECT_EQ(h3, ci_hash<u16string>()(str2));
        EXPECT_NE(std::hash<u16string>()(str1), std::hash<u16string>()(str2));
        // equal_to
        EXPECT_TRUE(ci_equal_to<ci_u16string>()(istr1, istr2));
        EXPECT_TRUE(std::equal_to<ci_u16string>()(istr1, istr2));
        EXPECT_TRUE(ci_equal_to<u16string>()(str1, str2));
        EXPECT_FALSE(std::equal_to<u16string>()(str1, str2));
    }

    // case-insensitive u16string map
    {
        std::map<u16string, int, ci_less<u16string>> cimap;
        cimap.emplace(u"abc", 1);
        cimap.emplace(u"XYZ", 2);
        EXPECT_EQ(1, cimap.count(u"ABC"));
        EXPECT_EQ(1, cimap.count(u"xyz"));
        cimap[u"ABC"] = -1;
        cimap[u"xyz"] = -2;
        EXPECT_EQ(-1, cimap[u"abc"]);
        EXPECT_EQ(-2, cimap[u"XYZ"]);
    }
    {
        std::map<ci_u16string, int> cimap;
        cimap.emplace(u"abc", 1);
        cimap.emplace(u"XYZ", 2);
        EXPECT_EQ(1, cimap.count(u"ABC"));
        EXPECT_EQ(1, cimap.count(u"xyz"));
        cimap[u"ABC"] = -1;
        cimap[u"xyz"] = -2;
        EXPECT_EQ(-1, cimap[u"abc"]);
        EXPECT_EQ(-2, cimap[u"XYZ"]);
    }

    // case-insensitive u16string hash table
    {
        std::unordered_map<u16string, int, ci_hash<u16string>, ci_equal_to<u16string>> hmap;
        hmap.emplace(u"abc", 1);
        hmap.emplace(u"XYZ", 2);
        EXPECT_EQ(1, hmap.count(u"ABC"));
        EXPECT_EQ(1, hmap.count(u"xyz"));
        hmap[u"ABC"] = -1;
        hmap[u"xyz"] = -2;
        EXPECT_EQ(-1, hmap[u"abc"]);
        EXPECT_EQ(-2, hmap[u"XYZ"]);
    }
    {
        std::unordered_map<ci_u16string, int> hmap;
        hmap.emplace(u"abc", 1);
        hmap.emplace(u"XYZ", 2);
        EXPECT_EQ(1, hmap.count(u"ABC"));
        EXPECT_EQ(1, hmap.count(u"xyz"));
        hmap[u"ABC"] = -1;
        hmap[u"xyz"] = -2;
        EXPECT_EQ(-1, hmap[u"abc"]);
        EXPECT_EQ(-2, hmap[u"XYZ"]);
    }
}
