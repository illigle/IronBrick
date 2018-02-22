/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an "as is" basis,
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable,
* fit for a particular purpose or non-infringing.

* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#ifndef _AVS_DECODER_H_
#define _AVS_DECODER_H_

#include "IrkMemPool.h"
#include "IrkVector.h"
#include "IrkSyncQueue.h"
#include "IrkThreadPool.h"
#include "IrkAvsDecoder.h"
#include "AvsDecUtility.h"
#include "AvsHeaders.h"
#include "AvsVlcParser.h"
#include "AvsAecParser.h"
#include "AvsIntraPred.h"
#include "AvsInterPred.h"

// opaque AVS+ decoder
struct IrkAvsDecoder
{
};

// 解码器允许的最大内部线程数, 如果创建的线程太多, 可能影响到系统其他模块, 同时占用过多内存
#define MAX_THEAD_CNT 16

namespace irk_avs_dec {

struct AvsContext;
struct DecFrame;
class  FrameFactory;
struct FrmDecContext;
class  FrmCtxFactory;

extern const int32_t g_DequantScale[64];
extern const uint8_t g_DequantShift[64];
extern const uint8_t g_ChromaQp[64 + 16];

//======================================================================================================================

// 针对并行解码, 请求参考帧数据
struct RefDataReq
{
    RefDataReq() : m_refLine(0), m_NotifyEvt(true) {}
    volatile int    m_refLine;          // 请求参考帧 m_refLine 行解码完成
    irk::SyncEvent  m_NotifyEvt;        // 参考帧已满足请求通知
};

// 针对并行解码, 管理当前帧解码进度
struct DecodingState
{
public:
    DecodingState() : m_lineReady(0), m_reqCnt(0), m_doneEvt(true)
    {
        memset(m_reqList, 0, sizeof(m_reqList));
    }
    ~DecodingState()
    {
        assert(m_reqCnt == 0);
    }

    // 更新当前帧解码进度
    void update_state(int line);

    // 添加参考数据请求, 等待请求满足
    void wait_request(RefDataReq* req);

    // 标识当前帧解码完成, 所有请求者返回
    void set_frame_done()
    {
        this->update_state(INT32_MAX);
        m_doneEvt.set();
        assert(m_reqCnt == 0);
    }

    // 等待当前帧解码完成
    void wait_frame_done()
    {
        if (m_lineReady != INT32_MAX)
            m_doneEvt.wait();
        assert(m_reqCnt == 0);
    }

    void reset()
    {
        assert(m_reqCnt == 0);
        m_lineReady = 0;
        m_reqCnt = 0;
        memset(m_reqList, 0, sizeof(m_reqList));
        m_doneEvt.reset();
    }

    volatile int    m_lineReady;                // 已解码的行数
    int             m_reqCnt;                   // 当前参考数据请求总数
    RefDataReq*     m_reqList[MAX_THEAD_CNT];   // 参考数据请求列表
    irk::Mutex      m_mutex;                    // 内部互斥体
    irk::SyncEvent  m_doneEvt;                  // 当前帧解码完成事件
};

//======================================================================================================================

// 解码帧工厂, 用以管理解码后 YUV 帧内存
class FrameFactory
{
public:
    FrameFactory();
    ~FrameFactory();

    // 设置自定义内存分配函数
    void set_alloc_callback(PFN_CodecAlloc pfnAlloc, void* cbparam);
    void set_dealloc_callback(PFN_CodecDealloc pfnDealloc, void* cbparam);

    // 配置视频帧大小
    bool config(int width, int height, int chromaFmt);

    // 创建一帧 DecFrame, isRef: 是否为参考帧
    DecFrame* create(bool isRef);

    // 丢弃 DecFrame
    void discard(DecFrame* pFrame);

private:
    static const int kCacheSize = MAX_THEAD_CNT;

    PFN_CodecAlloc      m_pfnAlloc;                 // 自定义内存分配函数
    void*               m_allocParam;               // 自定义内存分配函数的用户私有参数
    PFN_CodecDealloc    m_pfnDealloc;               // 自定义内存释放函数
    void*               m_deallocParam;             // 自定义内存释放函数的用户私有参数
    bool                m_bDefAlloc;                // 是否使用缺省内存分配

    int                 m_chromaFmt;                // 色差格式
    int                 m_lumaWidth;                // 亮度分量宽度
    int                 m_lumaHeight;               // 亮度分量高度
    int                 m_lumaPitch;                // 亮度分量行宽
    int                 m_lumaSize;
    int                 m_chromaWidth;              // 色差分量宽度
    int                 m_chromaHeight;             // 色差分量高度
    int                 m_chromaPitch;              // 色差分量行宽
    int                 m_chromaSize;
    int                 m_generation;               // 用以标记码流变化

    int                 m_frameCnt;                 // 当前缓存的 DecFrame 数目
    DecFrame*           m_frameCache[kCacheSize];
    int                 m_colMvCnt;                 // 当前缓存的 BDColMvs 数目
    BDColMvs*           m_colMvCache[kCacheSize];
    int                 m_decStateCnt;              // 当前缓存的 DecodingState 数目
    DecodingState*      m_decStateCache[kCacheSize];
    irk::Mutex          m_mutex;
    irk::MemSlots       m_pool;
};

// 解码后的一帧数据
struct DecFrame : IrkAvsDecedPic
{
    void add_ref()
    {
        irk::atomic_inc(&refCnt);
    }
    void dismiss()
    {
        assert(refCnt > 0 && factory != nullptr);
        if (irk::atomic_dec(&refCnt) == 0)
            factory->discard(this);
    }

    uint8_t         frameCoding;        // 0: 场编码, 1: 帧编码
    int32_t         poc;                // 显示顺序计数
    int16_t         denDistBD[2][4];    // 针对 B_Direct, 16384 / blockDistance
    BDColMvs*       colMvs;             // 针对 B_Direct, 存储当前帧的运动信息
    DecodingState*  decState;           // 当前帧解码进度

    volatile int    refCnt;             // 引用计数
    uint32_t        generation;         // 更改标记, 用于内存管理
    FrameFactory*   factory;            // 解码帧工厂
};

//======================================================================================================================

// 存储周边宏块的信息
struct MbContext
{
    int8_t      ipMode[2];          // 亮度分量帧内预测, -1 表示不可用
    int8_t      avail;              // 宏块是否可用
    uint8_t     cipFlag;            // 色度分量帧内预测模式为 Hor, Ver, Plane
    uint8_t     skipFlag;           // 是否为 P_SKip, B_Skip, B_Direct_16x16 宏块
    uint8_t     cbp;                // 当前宏块的 cbp
    uint8_t     notBD8x8[2];        // 8x8 块可用并且不是 B_Skip, B_Direct_16x16, SB_Direct_8x8
    int8_t      leftQp;             // 左边宏块的 qp 值
    int8_t      topQp;              // 上边宏块的 qp 值
    int8_t      curQp;              // 当前宏块的 qp 值
    uint32_t    lfBS;               // 环路滤波强度, 每个边界 2 比特
    int8_t      refIdxs[2][2];      // 参考帧索引, -1 表示不可用或帧内预测
    MvUnion     mvs[2][2];          // 运动矢量
};

struct SliceData
{
    uint8_t* data;
    int      size;
};
typedef irk::Vector<SliceData> SliceVector;
typedef irk::Vector<uint8_t> DataVector;

// 异步解码任务
class FrmDecTask : public irk::IAsyncTask
{
    typedef void(*PFN_DecFrame)(FrmDecContext* ctx);
public:
    FrmDecTask() : m_pfnDecFrame(nullptr), m_frameCtx(nullptr) {}
    void config(PFN_DecFrame pfnDec, FrmDecContext* ctx);
    void work() override;
private:
    PFN_DecFrame    m_pfnDecFrame;
    FrmDecContext*  m_frameCtx;
};

// 单帧解码 context
struct FrmDecContext
{
    FrmDecContext();
    ~FrmDecContext();

    AvsContext*     avsCtx;             // 全局解码 context
    AvsPicHdr       picHdr;             // picture header
    DataVector      dataBuf;            // 内部缓存
    SliceVector     sliceVec;           // 分割出的 slice, 并行解码用
    int64_t         userPts;            // 用户私有数据
    void*           userData;           // 用户私有数据
    DecFrame*       curFrame;           // 当前帧

    int             picWidth;           // 帧或场宽度, 16 的整数倍
    int             picHeight;          // 帧或场高度, 16 的整数倍
    int             chromaFmt;          // 色差格式
    int             chromaWidth;        // 色差分量宽度
    int             chromaHeight;       // 色差分量高度
    int8_t          pFieldSkip;         // PFieldSkip
    int8_t          bFieldEnhance;      // BFieldEnhanced

    uint8_t*        picPlane[3];        // current picture's buffer
    int             picPitch[3];        // current picture's pitch
    int             mbColCnt;           // 水平宏块数
    int             mbRowCnt;           // 垂直宏块数
    int             maxRefIdx;          // 最大参考帧索引
    uint8_t         frameCoding;        // 0: 场编码, 1: 帧编码
    uint8_t         fieldIdx;           // 0: 第一场, 1: 第二场
    DecFrame*       refFrames[2];       // 当前帧使用的参考帧
    RefPicture      refPics[4];         // 当前帧/场使用的参考帧/场
    int             distBuf[16];        // 用以存储 BlockDistance 等信息
    int*            refDist;            // 参考帧距离 BlockDistance
    int*            denDist;            // 512 / BlockDistance
    int             backIdxXor;         // 用以得到后向参考帧索引
    int             backMvScale[4];     // B 帧后向预测缩放系数

    AvsBitStream    bitsm;              // 码流读取器
    AvsVlcParser*   vlcParser;          // 变长编码解析器
    AvsAecParser*   aecParser;          // 高级熵编码解析器

    int16_t         mvdA[2][2];         // 用于 mv_diff 高级熵解码
    int             bFixedQp;           // 是否为固定 qp
    int             prevQpDelta;        // PreviousDeltaQP
    int             curQp;              // 当前宏块 qp
    uint8_t         sliceWPFlag;        // slice_weighting_flag
    uint8_t         mbWPFlag;           // mb_weighting_flag
    uint8_t         lumaScale[4];       // weighting inter pred
    int8_t          lumaDelta[4];
    uint8_t         cbcrScale[4];
    int8_t          cbcrDelta[4];
    int             lfDisabled;         // 是否禁用环路滤波
    int             errCode;            // 错误标识   
    int             mbTypeIdx;          // 宏块类型标识
    MbContext       leftMb;             // 左侧宏块
    MbContext*      topMbBuf[2];        // 宏块行存储区
    MbContext*      topLine;            // 上一宏块行
    MbContext*      curLine;            // 当前宏块行
    BDColMvs*       colMvs;             // 针对 B_Direct 运动估计, 存储运动信息
    uint8_t*        tempBuf;            // 临时内存, 2K 字节大小
    uint8_t*        mcBuff;             // 帧间预测临时, 大小 1024 字节
    const uint8_t*  invScan;            // 逆扫描矩阵
    uint8_t*        wqMatrix;           // weight quant matrix
    int16_t*        coeff;              // 残差系数临时内存, 大小 128 字节  
    Rect            refRcLuma;          // 亮度分量的有效参考范围
    Rect            refRcCbcr;          // 色差分量的有效参考范围
    RefDataReq      refRequest;         // 参考帧数据请求
    FrmDecTask*     decTask;            // 异步解码任务
};

// 管理 FrmDecContext
class FrmCtxFactory
{
public:
    FrmCtxFactory();
    ~FrmCtxFactory();

    // 创建单帧解码 context
    FrmDecContext* create();

    // 丢弃单帧解码 context
    void discard(FrmDecContext* frmCtx);

private:
    static const int kCacheSize = MAX_THEAD_CNT;
    int             m_ctxCnt;
    FrmDecContext*  m_ctxCache[kCacheSize];
};

//======================================================================================================================

// 帧内预测函数原型
typedef void(*PFN_IntraPred)(uint8_t* dst, int pitch, NBUsable);

// slice 解码函数原型
typedef void(*PFN_DecodeSlice)(FrmDecContext*, const uint8_t* data, int size);

// macroblock 解码函数原型
typedef void(*PFN_DecodeMB)(FrmDecContext*, int mx, int my);

// 解码器状态
#define AVS_SEQ_HDR_PARSED  1   // sequence header parsed

// 全局解码 context
struct AvsContext : IrkAvsDecoder
{
    AvsContext(const IrkAvsDecConfig* cfg, int sseVer);
    ~AvsContext();

    // 解码器初始化
    bool setup();

    // 丢弃已有的解码数据
    void clear();

    IrkAvsDecConfig config;                 // 用户配置
    int             sseVersion;             // CPU 支持的 SSE/AVX 版本
    PFN_CodecNotify pfnNotify;              // 解码回调函数
    void*           notifyParam;            // 解码回调函数用户私有数据

    PFN_IntraPred   pfnLumaIPred[5];        // 亮度分量帧内预测函数
    PFN_IntraPred   pfnCbCrIPred[4];        // 色差分量帧内预测函数
    PFN_DecodeMB    pfnDecMbP[5];           // P 宏块解码函数
    PFN_DecodeMB    pfnDecMbB[24];          // B 宏块解码函数
    PFN_DecodeMB    pfnDecMbP_AEC[5];       // P 宏块解码函数, 针对高级熵编码
    PFN_DecodeMB    pfnDecMbB_AEC[24];      // B 宏块解码函数, 针对高级熵编码

    int             threadCnt;              // 解码使用的线程数
    int             status;                 // 解码器状态
    AvsSeqHdr       seqHdr;                 // sequence header
    int             frameWidth;             // 视频帧宽度, 进位到宏块的整数倍
    int             frameHeight;            // 视频帧高度, 进位到宏块的整数倍
    int             skipNonRef;             // 是否跳过非参考帧
    DecFrame*       refFrames[2];           // 全局最新参考帧
    DecFrame*       outFrame;               // 待输出上一个参考帧

    FrameFactory    frmFactory;             // 解码帧工厂
    FrmCtxFactory   ctxFactory;             // 解码 context 工厂

    irk::SyncedQueue<FrmDecContext*>        workingQueue;   // 正在解码的 picture context
    irk::ThreadPool                         threadPool;     // 并行解码线程池
};

}   // namespace irk_avs_dec
#endif
