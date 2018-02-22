#include "gtest/gtest.h"
#include "IrkStringUtility.h"
#include "IrkSyncUtility.h"
#include "IrkEnvUtility.h"
#include "IrkProcess.h"
#include "IrkIPC.h"

using namespace irk;

#define CREATE_NEW_CONSOLE 0x00000010

namespace {

struct IpcMessage
{
    int id;
    int seq;
    int param;
    char data[256];
};

}

static const char* s_Text[18] =
{
    "1.Beautiful is better than ugly.",
    "2.Explicit is better than implicit.",
    "3.Simple is better than complex.",
    "4.Complex is better than complicated.",
    "5.Flat is better than nested.",
    "6.Sparse is better than dense.",
    "7.Readability counts.",
    "8.Special cases aren't special enough to break the rules.",
    "9.Although practicality beats purity.",
    "10.Errors should never pass silently.",
    "11.Unless explicitly silenced.",
    "12.In the face of ambiguity, refuse the temptation to guess.",
    "13.There should be one-- and preferably only one --obvious way to do it.",
    "14.Although that way may not be obvious at first unless you're Dutch.",
    "15.Now is better than never.",
    "16.Although never is often better than *right* now.",
    "17.If the implementation is hard to explain, it's a bad idea.",
    "18.If the implementation is easy to explain, it may be a good idea.",
};

#define ssizeof(x) (int)sizeof(x)

TEST(IPC, Semaphore)
{
#ifdef _WIN32
    const char* semaName = "IrkTestSema";
#else
    const char* semaName = "/IrkTestSema";
#endif

    Semaphore sema;
    Semaphore sema2;
    IpcResult rst = sema.create(semaName, 1);
    IpcResult rst2 = sema2.open(semaName);
    EXPECT_TRUE(rst && rst2);

    bool res = sema2.try_wait();
    EXPECT_TRUE(res);
    res = sema2.try_wait();
    EXPECT_FALSE(res);
    rst = sema2.wait(100);
    EXPECT_TRUE(rst.is_timeout());

    sema.post();
    rst = sema2.wait(100);
    EXPECT_TRUE(rst);
}

TEST(IPC, freight)
{
    std::string exePath = get_proc_dir();
#ifdef _WIN32
    exePath += '\\';
    exePath += "IPCTest.exe";
    const char* serverName = "Local\\IrkIpcTruck";
    const char* semaName = "Local\\IrkIpcTruckSema";
#else
    exePath += '/';
    exePath += "IPCTest";
    const char* serverName = "/IrkIpcTruck";
    const char* semaName = "/IrkIpcTruckSema";
#endif

    CmdLine cmdline;
    cmdline.set_exepath(exePath.c_str());
    cmdline.add_opt("-freight", serverName);
    cmdline.add_opt("-sema", semaName);

    Semaphore readySema;
    OSProcess procServer;
    ASSERT_TRUE(readySema.create(semaName));
    ASSERT_TRUE(procServer.launch(cmdline) == 0);
    if (!readySema.wait(2000))   // wait server ready
    {
        fprintf(stderr, "wait server ready failed\n");
        return;
    }

    IpcFreightClient client;
    IpcResult rst = client.open(serverName);
    EXPECT_TRUE(rst);
    if (!rst)
    {
        fprintf(stderr, "connect cargo server %s failed %d\n", serverName, rst.errc);
        procServer.kill();
        return;
    }

    std::string tmpStr;
    IpcCargo cargo;
    for (int i = 0; i < 10; i++)
    {
        // create a cargo
        rst = client.create(cargo);
        EXPECT_TRUE(rst);
        EXPECT_TRUE(((uintptr_t)cargo.data & 15) == 0);
        if (!rst)
        {
            fprintf(stderr, "alloc cargo failed %d\n", rst.errc);
            break;
        }
        // fill cargo with data
        ASSERT_TRUE(cargo.data);
        str_copy((char*)cargo.data, cargo.capacity, s_Text[i]);
        cargo.size = (int)strlen(s_Text[i]);

        // send cargo
        rst = client.send(cargo);
        EXPECT_TRUE(rst);
        if (!rst)
        {
            fprintf(stderr, "send cargo failed %d\n", rst.errc);
            break;
        }

        // receive ack
        rst = client.recv(cargo, 1000);
        EXPECT_TRUE(rst);
        EXPECT_TRUE(((uintptr_t)cargo.data & 15) == 0);
        if (!rst)
        {
            fprintf(stderr, "recv cargo failed %d\n", rst.errc);
            break;
        }
        tmpStr = get_uppered(s_Text[i]);
        EXPECT_EQ((int)tmpStr.length(), cargo.size);
        EXPECT_STREQ(tmpStr.c_str(), (char*)cargo.data);

        // discard ack cargo
        client.discard(cargo);
    }

    // send requests in bulk
    for (int i = 0; i < 10; i++)
    {
        // alloc a cargo
        rst = client.create(cargo);
        EXPECT_TRUE(rst);
        EXPECT_TRUE(((uintptr_t)cargo.data & 15) == 0);
        if (!rst)
        {
            fprintf(stderr, "alloc cargo failed %d\n", rst.errc);
            break;
        }
        // fill cargo with data
        ASSERT_TRUE(cargo.data);
        str_copy((char*)cargo.data, cargo.capacity, s_Text[i]);
        cargo.size = (int)strlen(s_Text[i]);

        // send cargo
        rst = client.send(cargo);
        EXPECT_TRUE(rst);
        if (!rst)
        {
            fprintf(stderr, "send cargo failed %d\n", rst.errc);
            break;
        }
    }

    // receive acks in bulk
    char tempBuf[256] = {};
    for (int i = 0; i < 10; i++)
    {
        rst = client.recv(cargo, 1000);
        EXPECT_TRUE(rst);
        EXPECT_TRUE(((uintptr_t)cargo.data & 15) == 0);
        if (!rst)
        {
            fprintf(stderr, "recv cargo failed %d\n", rst.errc);
            break;
        }
        tmpStr = get_uppered(s_Text[i]);
        EXPECT_EQ((int)tmpStr.length(), cargo.size);
        EXPECT_STREQ(tmpStr.c_str(), (char*)cargo.data);

        // discard ack cargo
        client.discard(cargo);
    }

    // send exit cargo
    rst = client.create(cargo);
    if (rst && cargo.data)
    {
        strcpy((char*)cargo.data, "exit");
        cargo.size = 4;
        rst = client.send(cargo);
    }

    procServer.wait();
}

TEST(IPC, msgq)
{
    std::string exePath = get_proc_dir();
#ifdef _WIN32
    exePath += '\\';
    exePath += "IPCTest.exe";
    const char* mqName = "\\\\.\\pipe\\IrkIpcMqTest";
    const char* semaName = "Local\\IrkMqT3Sema";
#else
    exePath += '/';
    exePath += "IPCTest";
#ifdef __linux__
    const char* mqName = "/IrkIpcMqTest";
#else
    const char* mqName = "/tmp/IrkIpcMqTest";
#endif
    const char* semaName = "/IrkMqT3Sema";
#endif

    CmdLine cmdline;
    cmdline.set_exepath(exePath.c_str());
    cmdline.add_opt("-mq", mqName);
    cmdline.add_opt("-sema", semaName);

    Semaphore readySema;
    OSProcess procServer;
    ASSERT_TRUE(readySema.create(semaName));
    ASSERT_TRUE(procServer.launch(cmdline) == 0);
    if (!readySema.wait(2000))   // wait server ready
    {
        fprintf(stderr, "wait server ready failed\n");
        return;
    }

    IpcMqClient client;
    IpcResult rst = client.connect(mqName, 2000);
    EXPECT_TRUE(rst);
    if (!rst)
    {
        fprintf(stderr, "connect mq server %s failed %d\n", mqName, rst.errc);
        procServer.kill();
        return;
    }

    std::string tmpStr;
    IpcMessage msg = {};
    IpcMessage msg2 = {};
    for (int i = 0; i < 10; i++)
    {
        msg.id = 0;
        msg.param = 0;
        msg.seq = i + 1;
        str_copy(msg.data, sizeof(msg.data), s_Text[i]);

        memset(&msg2, 0, sizeof(msg2));
        int recvSize = -1;
        rst = client.transact(&msg, ssizeof(msg), &msg2, ssizeof(msg2), recvSize, 1000);
        EXPECT_TRUE(rst);
        if (!rst || recvSize != ssizeof(msg2))
        {
            fprintf(stderr, "mq transact failed %d\n", rst.errc);
            break;
        }

        tmpStr = get_uppered(s_Text[i]);
        EXPECT_EQ(msg.seq * 10, msg2.seq);
        EXPECT_EQ((int)tmpStr.length(), msg2.param);
        EXPECT_STREQ(tmpStr.c_str(), msg2.data);
    }

    // send request in bulk
    for (int i = 0; i < 10; i++)
    {
        msg.id = 1;
        str_copy(msg.data, sizeof(msg.data), s_Text[i]);

        rst = client.send(&msg, ssizeof(msg), 500);
        EXPECT_TRUE(rst);
        if (!rst)
        {
            fprintf(stderr, "mq send failed %d\n", rst.errc);
            break;
        }
    }

    // receive ack in bulk
    char tempBuf[512] = {};
    for (int i = 0; i < 10; i++)
    {
        int recvSize = -1;
        rst = client.recv(tempBuf, ssizeof(tempBuf), recvSize, 500);
        EXPECT_TRUE(rst);
        if (!rst)
        {
            fprintf(stderr, "mq recv failed %d\n", rst.errc);
            break;
        }
        tmpStr = get_uppered(s_Text[i]);
        EXPECT_EQ((int)tmpStr.length() + 1, recvSize);
        EXPECT_STREQ(tmpStr.c_str(), tempBuf);
    }

    // send exit command
    msg.id = 0;
    msg.seq = -1001;
    ::strcpy(msg.data, "EXIT");
    client.send(&msg, ssizeof(msg));
    client.disconnect();

    procServer.wait();
}

TEST(IPC, pipe)
{
    std::string exePath = get_proc_dir();
#ifdef _WIN32
    exePath += '\\';
    exePath += "IPCTest.exe";
    const char* pipeName = "\\\\.\\pipe\\IrkIpcPipe";
    const char* semaName = "Local\\IrkPipe3Sema";
#else
    exePath += '/';
    exePath += "IPCTest";
    const char* pipeName = "/tmp/IrkIpcPipe";
    const char* semaName = "/IrkPipe3Sema";
#endif

    CmdLine cmdline;
    cmdline.set_exepath(exePath.c_str());
    cmdline.add_opt("-pipe", pipeName);
    cmdline.add_opt("-sema", semaName);

    Semaphore readySema;
    OSProcess procServer;
    ASSERT_TRUE(readySema.create(semaName));
    ASSERT_TRUE(procServer.launch(cmdline) == 0);
    if (!readySema.wait(2000))   // wait server ready
    {
        fprintf(stderr, "wait server ready failed\n");
        return;
    }

    IpcPipeClient client;
    IpcResult rst = client.connect(pipeName, 2000);
    EXPECT_TRUE(rst);
    if (!rst)
    {
        fprintf(stderr, "connect pipe server %s failed %d\n", pipeName, rst.errc);
        procServer.kill();
        return;
    }

    // send
    int sentSize = 0;
    for (int i = 0; i < 10; i++)
    {
        int len = (int)strlen(s_Text[i]);
        rst = client.send(s_Text[i], len, 500);
        EXPECT_TRUE(rst);
        if (!rst)
        {
            fprintf(stderr, "pipe send failed %d\n", rst.errc);
            break;
        }
        sentSize += len;
    }

    // receive
    std::string recvStr;
    int recvSize = 0;
    char tempBuf[256];
    while (recvSize < sentSize)
    {
        int cnt = -1;
        rst = client.recv(tempBuf, ssizeof(tempBuf), cnt, 500);
        EXPECT_TRUE(rst);
        if (!rst)
        {
            fprintf(stderr, "recv failed %d\n", rst.errc);
            break;
        }
        recvStr.append(tempBuf, cnt);
        recvSize += cnt;
    }

    std::string rope;
    for (int i = 0; i < 10; i++)
        rope += get_uppered(s_Text[i]);
    EXPECT_EQ(rope.length(), sentSize);
    EXPECT_EQ(rope.length(), recvSize);
    EXPECT_EQ(rope, recvStr);

    client.disconnect();
    procServer.wait();
}
