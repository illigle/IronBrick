#include "AvsFileReader.h"
#include "IrkAvsDecoder.h"
#include <chrono>

static void avs_dec_notifier(int code, void* data, void* cbparam)
{
    if (code == IRK_CODEC_DONE) // 解码出一帧视频, 输出 YUV 数据
    {
        const IrkAvsDecedPic* pframe = (const IrkAvsDecedPic*)data;
        FILE* fp = (FILE*)cbparam;
        if (fp)
        {
            const uint8_t* src = pframe->plane[0];
            int width = pframe->width[0];
            int height = pframe->height[0];
            int pitch = pframe->pitch[0];
            for (int i = 0; i < height; i++)
            {
                fwrite(src, 1, width, fp);
                src += pitch;
            }
            src = pframe->plane[1];
            width = pframe->width[1];
            height = pframe->height[1];
            pitch = pframe->pitch[1];
            for (int i = 0; i < height; i++)
            {
                fwrite(src, 1, width, fp);
                src += pitch;
            }
            src = pframe->plane[2];
            width = pframe->width[2];
            height = pframe->height[2];
            pitch = pframe->pitch[2];
            for (int i = 0; i < height; i++)
            {
                fwrite(src, 1, width, fp);
                src += pitch;
            }
            fflush(fp);
        }
    }
    else    // 其他通知事件
    {
    }
}

const char* s_avsFileName = "E:/Clips/avs/sudu.avs";
const char* s_yuvFileName = "E:/avs_dec.yuv";

void test_avs_decoder()
{
    // 打开 AVS 裸流文件
    AvsFileReader reader;
    if (!reader.open(s_avsFileName))
    {
        fprintf(stderr, "openf file %s failed\n", s_avsFileName);
        return;
    }

    // 创建解码器
    IrkAvsDecConfig cfg = {0};
    IrkAvsDecoder* decoder = irk_create_avs_decoder(&cfg);
    if (!decoder)
    {
        fprintf(stderr, "irk_create_avs_decoder failed\n");
        return;
    }

    // 输出 YUV 文件
    FILE* fpyuv = fopen(s_yuvFileName, "wb");

    // 设置解码回调函数
    irk_avs_decoder_set_notify(decoder, &avs_dec_notifier, fpyuv);

    auto startTime = std::chrono::high_resolution_clock::now();

    IrkCodedPic encPic = {0};
    int frmCnt = 0;
    while (frmCnt < 100)
    {
        // 读取一帧数据
        encPic.data = (uint8_t*)reader.get_picture(&encPic.size);
        if (!encPic.data)
            break;
        encPic.userpts = 1 * frmCnt++;

        // 解码
        if (irk_avs_decoder_decode(decoder, &encPic) <= 0)
            break;
    }

    IrkAvsStreamInfo info;
    if (irk_avs_decoder_get_info(decoder, &info) == 0)
    {
        printf("resolution: %dx%d\n", info.width, info.height);
    }

    // 输出缓存的帧
    irk_avs_decoder_decode(decoder, NULL);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elpased = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // 关闭解码器
    irk_destroy_avs_decoder(decoder);

    // 平均解码时间, 包括磁盘读写
    if (frmCnt > 0)
        fprintf(stderr, "\naverage dec time : %0.3f ms\n", 0.001 * elpased.count() / frmCnt);

    if (fpyuv)
        fclose(fpyuv);
}
