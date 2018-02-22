#include <random>
#include "gtest/gtest.h"
#include "IrkBase64.h"
#include "IrkHMAC.h"

TEST(Base64, Base64)
{
    std::random_device rnddev;
    std::minstd_rand rnd;
    rnd.seed(rnddev());

    const int dataSize = 256 + (int)(rnd() & 0xFF);
    uint8_t* dataRaw = new uint8_t[dataSize];
    uint8_t* dataDec = new uint8_t[dataSize];
    for (int i = 0; i < dataSize; i++)
        dataRaw[i] = (uint8_t)(rnd() & 0xFF);

    const int bufSize = 4 * (dataSize / 3 + 1);
    char* b64Buf = new char[bufSize];

    int cnt = irk::encode_base64(dataRaw, 0, b64Buf, bufSize);
    EXPECT_EQ(0, cnt);

    cnt = irk::encode_base64(dataRaw, 1, b64Buf, bufSize);
    EXPECT_EQ(4, cnt);
    cnt = irk::decode_base64(b64Buf, cnt, dataDec, dataSize);
    EXPECT_EQ(1, cnt);
    EXPECT_TRUE(memcmp(dataRaw, dataDec, cnt) == 0);

    cnt = irk::encode_base64(dataRaw, 2, b64Buf, bufSize);
    EXPECT_EQ(4, cnt);
    cnt = irk::decode_base64(b64Buf, cnt, dataDec, dataSize);
    EXPECT_EQ(2, cnt);
    EXPECT_TRUE(memcmp(dataRaw, dataDec, cnt) == 0);

    cnt = irk::encode_base64(dataRaw, 3, b64Buf, bufSize);
    EXPECT_EQ(4, cnt);
    cnt = irk::decode_base64(b64Buf, cnt, dataDec, dataSize);
    EXPECT_EQ(3, cnt);
    EXPECT_TRUE(memcmp(dataRaw, dataDec, cnt) == 0);

    for (int len = 5; len < dataSize; len += len)
    {
        cnt = irk::encode_base64(dataRaw, len, b64Buf, bufSize);
        EXPECT_GE(4 * (len / 3 + 1), cnt);

        memset(dataDec, 0, dataSize);
        cnt = irk::decode_base64(b64Buf, cnt, dataDec, dataSize);
        EXPECT_EQ(len, cnt);
        EXPECT_TRUE(memcmp(dataRaw, dataDec, cnt) == 0);
    }

    std::string str = irk::encode_base64(dataRaw, dataSize);
    EXPECT_GE(4 * (dataSize / 3 + 1), (int)str.length());

    memset(dataDec, 0, dataSize);
    cnt = irk::decode_base64(str, dataDec, dataSize);
    EXPECT_EQ(dataSize, cnt);
    EXPECT_TRUE(memcmp(dataRaw, dataDec, dataSize) == 0);

    delete[] dataRaw;
    delete[] dataDec;
    delete[] b64Buf;
}

TEST(Base64, Base64_Hmac_Sha1)
{
    const char* key = "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV";
    const char* text = "PUT\nODBGOERFMDMzQTczRUY3NUE3NzA5QzdFNUYzMDQxNEM=\ntext/html\n"
        "Thu, 17 Nov 2005 18:49:58 GMT\n"
        "x-oss-magic:abracadabra\n"
        "x-oss-meta-author:foo@bar.com\n"
        "/oss-example/nelson";

    uint8_t mac[20];
    irk::hmac_sha1(key, (uint32_t)strlen(key), text, strlen(text), mac);
    std::string authStr = irk::encode_base64(mac, 20);
    EXPECT_STREQ("26NBxoKdsyly4EDv6inkoDft/yA=", authStr.c_str());
}
