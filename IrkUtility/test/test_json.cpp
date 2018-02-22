#include "gtest/gtest.h"
#include "IrkJSON.h"
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
    FsPath(const std::string& str) : std::string(str) {}
    FsPath(std::string&& str) : std::string(std::move(str)) {}

    void operator /=(const char* leaf)
    {
        *this += kSlash;
        *this += leaf;
    }
    void operator /=(const std::string& leaf)
    {
        *this += kSlash;
        *this += leaf;
    }
    std::string filename() const
    {
        size_t pos = this->rfind(kSlash);
        if (pos != std::string::npos)
            return this->substr(pos + 1);
        return std::string{};
    }
};

}

static FsPath get_testdir()
{
    FsPath testDir(get_home_dir());
    testDir /= "TestData";
    testDir /= "json";
    return testDir;
}

static FsPath get_outdir()
{
    FsPath outputDir(get_home_dir());
    outputDir /= "TestData";
    outputDir /= "json";
    outputDir /= "_output";
    if (!irk_fexist(outputDir.c_str()))
        irk_mkdir(outputDir.c_str());
    return outputDir;
}

TEST(Json, Create)
{
    JsonDoc doc;
    EXPECT_FALSE((bool)doc);

    // create rootAry
    JsonArray& rootAry = doc.create_root_array();
    EXPECT_TRUE((bool)doc);

    // add element to JSON Array
    EXPECT_TRUE(rootAry.count() == 0);
    rootAry.add(nullptr);
    rootAry.add(false);
    rootAry.add(1);
    rootAry.add(2u);
    rootAry += (int64_t)-1;
    rootAry += UINT64_MAX;
    rootAry += 1.123456789;
    rootAry += 2.0f;
    rootAry += "abc";
    rootAry += string("xyz");
    rootAry += {-1, -2, -3, -4, -5, -6};
    rootAry += std::make_tuple("123", 123, true, nullptr, 101.23);
    JsonObject& jobj = rootAry.add_object();

    // element search
    JsonElem& elem = rootAry[3];
    EXPECT_TRUE(elem.is_number());

    // get element value
    int64_t val = 0;
    EXPECT_EQ(2, elem.as_int());
    EXPECT_EQ(2, elem.as_uint());
    EXPECT_EQ(2.0, elem.as_double());
    EXPECT_TRUE(elem.get(val));
    EXPECT_EQ(2, val);
    EXPECT_THROW(elem.as_string(), JsonBadAccess);

    // element assignment
    elem = 12;
    EXPECT_EQ(12, elem.as_uint());
    elem = false;
    EXPECT_FALSE(elem.as_bool());

    // array traversal, ranged-for
    const char* checkStr = "null string";
    int cnt = 0;
    for (auto& val : rootAry)
    {
        if (val.is_null())
            val = checkStr;
        cnt++;
    }
    EXPECT_EQ(cnt, rootAry.count());
    JsonElem* child = rootAry.first();
    EXPECT_STREQ(checkStr, child->as_string());

    // array traversal, iterator
    int objCnt = 0;
    for (auto itr = rootAry.begin(); itr != rootAry.end(); ++itr)
    {
        if (itr->is_object())
            objCnt++;
    }
    EXPECT_EQ(1, objCnt);

    // add member to JSON Object
    EXPECT_TRUE(jobj.count() == 0);
    jobj.add("1", 1);
    jobj.add("a", 'a');
    jobj["path"] = "C:\\very-very-very-long-dir\\log.txt";
    jobj["speed"] = 3600.f;
    jobj["person"] = JsonObject{};
    EXPECT_EQ(5, jobj.count());

    // delete member
    bool res = jobj.erase("a");
    EXPECT_TRUE(res);
    EXPECT_EQ(4, jobj.count());

    // find member
    JsonMember* pmemb = jobj.find("person");
    EXPECT_TRUE(pmemb != nullptr && pmemb->is_object());
    EXPECT_EQ(&jobj, pmemb->parent());
    JsonObject& person = pmemb->as_object();
    EXPECT_TRUE(person.count() == 0);
    person.add("phone-number", "1234567890");
    person += std::make_pair("address", "shenzhen-china");

    // object traversal
    int itemCnt = 0;
    for (const auto& member : jobj)
    {
        printf("%s\n", member.name());
        itemCnt++;
    }
    EXPECT_EQ(itemCnt, jobj.count());

    // write to file
    FsPath outfile1(get_outdir());
    outfile1 /= "test1.json";
    doc.dump_file(outfile1.c_str());

    // create new JSON document
    JsonObject& rootObj = doc.create_root_object();
    EXPECT_EQ(&rootObj, &doc.root().as_object());
    rootObj.adds(
        "name", "fat-man",
        "age", 18,
        std::string("gender"), "male",
        "empty", JsonObject{}
    );
    rootObj.adds("moneny", 99999999);
    rootObj["name"] = "slim";
    rootObj["address"] = u8"深圳";
    rootObj["title"] = JsonArray{};
    JsonArray& title = rootObj["title"].as_array();
    title.adds(1, "boss", 2, "manager", JsonObject{});

    JsonArray& ary = rootObj.add_array("details");
    ary.adds(1, -2, 3.1415926, nullptr, true, "c-string", string("std::string"));
    ary.add(u8"小小");
    ary += std::make_tuple(101, "abc", 99.99);

    FsPath outfile2(get_outdir());
    outfile2 /= "test2.json";
    doc.pretty_dump_file(outfile2.c_str());
}

TEST(Json, Create2)
{
    JsonMempoolAllocator allocator;

    JsonObject obj(&allocator);
    obj.add("1", 1);
    obj.add(std::make_pair("2", 2.0));
    obj += std::make_pair("3", true);
    obj.adds(
        "4", "4000",
        "5", nullptr,
        "6", JsonObject{},
        "7", JsonArray{}
    );

    std::string text1 = json_dump(obj);
    JsonValue val = json_parse(text1.c_str());
    EXPECT_TRUE(val.is_object());
    std::string text2 = json_dump(val);
    EXPECT_EQ(text1, text2);

    JsonArray ary(&allocator);
    ary.add(1);
    ary += 2.0;
    ary += {"123", "456", "789"};
    ary += std::make_tuple(-1, -2.9, "xyz", true, nullptr, JsonObject{});
    ary.resize(16);

    text1 = json_pretty_dump(ary);
    val = json_parse(text1.c_str());
    EXPECT_TRUE(val.is_array());
    text2 = json_pretty_dump(val);
    EXPECT_EQ(text1, text2);
}

static void parse_json_file(const FsPath& srcfile)
{
    JsonDoc doc;
    if (doc.load_file(srcfile.c_str()) < 0)
    {
        // only fail*.json should failed
        string filename = srcfile.filename();
        if (filename.find("fail") == string::npos)
        {
            printf("parse json %s failed\n", filename.c_str());
            EXPECT_TRUE(false);
        }
        return;
    }

    JsonValue& root = doc.root();
    if (root.is_array())
    {
        JsonArray& ary = root.as_array();
        ary += "NEW element";
    }
    else if (root.is_object())
    {
        JsonObject& obj = root.as_object();
        obj["NEW member"] = 10086;
    }

    FsPath outfile(get_outdir());
    outfile /= srcfile.filename();
    doc.pretty_dump_file(outfile.c_str());
}

TEST(Json, Parse)
{
    FsPath filePath;
    std::string dir = get_testdir();

    for (const DirEntry* entry : DirWalker(dir.c_str()))
    {
        if (::strcmp(entry->extension(), ".json") == 0)
        {
            filePath.assign(dir);
            filePath /= entry->name;
            parse_json_file(filePath);
        }
    }
}
