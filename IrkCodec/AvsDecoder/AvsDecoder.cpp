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

#include "AvsDecoder.h"
#include "IrkThread.h"

namespace irk_avs_dec {

// 标准表 62
extern const int32_t g_DequantScale[64] = 
{
    32768, 36061, 38968, 42495, 46341, 50535, 55437, 60424,
    32932, 35734, 38968, 42495, 46177, 50535, 55109, 59933,
    65535, 35734, 38968, 42577, 46341, 50617, 55027, 60097,
    32809, 35734, 38968, 42454, 46382, 50576, 55109, 60056,
    65535, 35734, 38968, 42495, 46320, 50515, 55109, 60076,
    65535, 35744, 38968, 42495, 46341, 50535, 55099, 60087,
    65535, 35734, 38973, 42500, 46341, 50535, 55109, 60097,
    32771, 35734, 38965, 42497, 46341, 50535, 55109, 60099,
};

// 标准表 62
extern const uint8_t g_DequantShift[64] =
{
    14, 14, 14, 14, 14, 14, 14, 14,
    13, 13, 13, 13, 13, 13, 13, 13,
    13, 12, 12, 12, 12, 12, 12, 12,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 10, 10, 10, 10, 10, 10, 10,
    10,  9,  9,  9,  9,  9,  9,  9,
    9,   8,  8,  8,  8,  8,  8,  8,
    7,   7,  7,  7,  7,  7,  7,  7
};

// 标准表 61
extern const uint8_t g_ChromaQp[64+16] = 
{
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 42, 43, 43, 44, 44,
    45, 45, 46, 46, 47, 47, 48, 48, 48, 49, 49, 49, 50, 50, 50, 51,
};

// 标准定义的转置, 用以实现解析后的 DCT 顺序按列存储
alignas(16) static uint8_t s_InvScan[64*2] = 
{
    // frame zig-zag scan
    0,  8,  1,  2,  9,  16, 24, 17, 10, 3,  4,  11, 18, 25, 32, 40, 
    33, 26, 19, 12, 5,  6,  13, 20, 27, 34, 41, 48, 56, 49, 42, 35, 
    28, 21, 14, 7,  15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30, 
    23, 31, 38, 45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63,
    // field zig-zag scan
    0,  1,  2,  8,  3,  4,  9,  10, 5,  6,  11, 16, 17, 7,  12, 18,
    24, 13, 14, 19, 25, 26, 32, 15, 20, 33, 21, 27, 34, 22, 28, 35,
    40, 41, 23, 29, 36, 42, 48, 43, 30, 37, 49, 50, 44, 31, 38, 51,
    45, 39, 52, 46, 53, 47, 54, 56, 55, 57, 58, 59, 60, 61, 62, 63,
};

alignas(16) static const uint8_t s_WQParam[4][8] = 
{
    { 128,  98, 106, 116, 116, 128 },
    { 135, 143, 143, 160, 160, 213 },
    { 128,  98, 106, 116, 116, 128 },
};

#define WQ_MODEL_0( wq ) { \
    wq[0], wq[0], wq[0], wq[4], wq[4], wq[4], wq[5], wq[5], \
    wq[0], wq[0], wq[3], wq[3], wq[3], wq[3], wq[5], wq[5], \
    wq[0], wq[3], wq[2], wq[2], wq[1], wq[1], wq[5], wq[5], \
    wq[4], wq[3], wq[2], wq[2], wq[1], wq[5], wq[5], wq[5], \
    wq[4], wq[3], wq[1], wq[1], wq[5], wq[5], wq[5], wq[5], \
    wq[4], wq[3], wq[1], wq[5], wq[5], wq[5], wq[5], wq[5], \
    wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], \
    wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], \
}; 

#define WQ_MODEL_1( wq ) { \
    wq[0], wq[0], wq[0], wq[4], wq[4], wq[4], wq[5], wq[5], \
    wq[0], wq[0], wq[4], wq[4], wq[4], wq[4], wq[5], wq[5], \
    wq[0], wq[3], wq[2], wq[2], wq[2], wq[1], wq[5], wq[5], \
    wq[3], wq[3], wq[2], wq[2], wq[1], wq[5], wq[5], wq[5], \
    wq[3], wq[3], wq[2], wq[1], wq[5], wq[5], wq[5], wq[5], \
    wq[3], wq[3], wq[1], wq[5], wq[5], wq[5], wq[5], wq[5], \
    wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], \
    wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], \
}; 

#define WQ_MODEL_2( wq ) { \
    wq[0], wq[0], wq[0], wq[4], wq[4], wq[3], wq[5], wq[5], \
    wq[0], wq[0], wq[4], wq[4], wq[3], wq[2], wq[5], wq[5], \
    wq[0], wq[4], wq[4], wq[3], wq[2], wq[1], wq[5], wq[5], \
    wq[4], wq[4], wq[3], wq[2], wq[1], wq[5], wq[5], wq[5], \
    wq[4], wq[3], wq[2], wq[1], wq[5], wq[5], wq[5], wq[5], \
    wq[3], wq[2], wq[1], wq[5], wq[5], wq[5], wq[5], wq[5], \
    wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], \
    wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], wq[5], \
}; 

//======================================================================================================================

// 更新当前帧解码进度
void DecodingState::update_state( int line )
{
    if( m_lineReady >= line )
        return;

    irk::Mutex::Guard guard_( m_mutex );

    m_lineReady = line;

    // 通知已满足条件的参考数据等待者
    if( m_reqCnt > 0 )
    {
        for( int i = 0; i < MAX_THEAD_CNT; i++ )
        {
            if( m_reqList[i] != nullptr && m_reqList[i]->m_refLine <= line )
            {
                m_reqList[i]->m_NotifyEvt.set();
                m_reqList[i] = nullptr;
                m_reqCnt--;
            }
        }
    }
    assert( m_reqCnt >= 0 );
}

// 添加参考数据请求, 等待请求满足
void DecodingState::wait_request( RefDataReq* req )
{
    if( m_lineReady >= req->m_refLine ) // 请求已满足, 快速返回
        return;

    req->m_NotifyEvt.reset();

    irk::Mutex::Guard guard_( m_mutex );

    if( m_lineReady >= req->m_refLine ) // 请求已满足, 快速返回
        return;

    // 添加请求到列表
    assert( m_reqCnt < MAX_THEAD_CNT );
    for( int i = 0; i < MAX_THEAD_CNT; i++ )
    {
        if( m_reqList[i] == nullptr )
        {
            m_reqList[i] = req;
            m_reqCnt++;
            break;
        }
    }
    guard_.unlock();

    // 等待请求条件满足
    req->m_NotifyEvt.wait();
}

//======================================================================================================================

// 缺省内存分配函数
static IrkMemBlock avs_default_alloc( size_t size, size_t alignment, void* )
{
    IrkMemBlock mblock;
    mblock.buf = irk::aligned_malloc( size, alignment );
    mblock.size = size;
    return mblock;
}

// 缺省内存释放函数
static void avs_default_dealloc( IrkMemBlock* mblock, void* )
{
    irk::aligned_free( mblock->buf );
    mblock->buf = nullptr;
    mblock->size = 0;
}

FrameFactory::FrameFactory()
{
    m_pfnAlloc = &avs_default_alloc;
    m_allocParam = nullptr;
    m_pfnDealloc = &avs_default_dealloc;
    m_deallocParam = nullptr;
    m_bDefAlloc = true;
    m_chromaFmt = 0;
    m_lumaWidth = 0;
    m_lumaHeight = 0;
    m_lumaPitch = 0;
    m_lumaSize = 0;
    m_chromaWidth = 0;
    m_chromaHeight = 0;
    m_chromaPitch = 0;
    m_chromaSize = 0;
    m_generation = 0;

    m_frameCnt = 0;
    memset( m_frameCache, 0, sizeof(m_frameCache) );
    m_colMvCnt = 0;
    memset( m_colMvCache, 0, sizeof(m_colMvCache) );
    m_decStateCnt = 0;
    memset( m_decStateCache, 0, sizeof(m_decStateCache) );

    m_pool.init( sizeof(DecFrame), kCacheSize, alignof(DecFrame) );
}

FrameFactory::~FrameFactory()
{
    for( int i = 0; i < m_frameCnt; i++ )
    {
        (*m_pfnDealloc)( &m_frameCache[i]->mblock, m_deallocParam );
    }
    for( int i = 0; i < m_colMvCnt; i++ )
    {
        delete[] m_colMvCache[i];
    }
    for( int i = 0; i < m_decStateCnt; i++ )
    {
        delete m_decStateCache[i];
    }
}

// 设置自定义内存分配函数
void FrameFactory::set_alloc_callback( PFN_CodecAlloc pfnAlloc, void* cbparam )
{
    m_pfnAlloc = pfnAlloc;
    m_allocParam = cbparam;
    m_bDefAlloc = false;
}
void FrameFactory::set_dealloc_callback( PFN_CodecDealloc pfnDealloc, void* cbparam )
{
    m_pfnDealloc = pfnDealloc;
    m_deallocParam = cbparam;
    m_bDefAlloc = false;
}

// 配置视频帧大小
bool FrameFactory::config( int width, int height, int chromaFmt )
{
    // 如果没有变化直接返回
    if( width == m_lumaWidth && height == m_lumaHeight && chromaFmt == m_chromaFmt )
        return true;

    m_chromaFmt = chromaFmt;
    m_lumaWidth = width;
    m_lumaHeight = height;
    m_lumaPitch = XmmPitch( width ) + 16;                       // 左右边界预留 8 个字节
    m_lumaSize = m_lumaPitch * YmmPitch( height );

    if( chromaFmt == AVS_CHROMA_420 )
    {
        m_chromaWidth = width >> 1;
        m_chromaHeight = height >> 1;
        m_chromaPitch = XmmPitch( m_chromaWidth ) + 16;         // 左右边界预留 8 个字节
        m_chromaSize = m_chromaPitch * YmmPitch( height ) >> 1;
    }
    else if( chromaFmt == AVS_CHROMA_422 )
    {
        m_chromaWidth = width >> 1;
        m_chromaHeight = height;
        m_chromaPitch = XmmPitch( m_chromaWidth ) + 16;         // 左右边界预留 8 个字节
        m_chromaSize = m_chromaPitch * YmmPitch( height );
    }
    else
    {
        return false;
    }

    // free old data if exists
    if( m_frameCnt > 0 )
    {
        for( int i = 0; i < m_frameCnt; i++ )
        {
            (*m_pfnDealloc)( &m_frameCache[i]->mblock, m_deallocParam );
            m_pool.dealloc( m_frameCache[i] );
            m_frameCache[i] = nullptr;
        }
        m_frameCnt = 0;

        for( int i = 0; i < m_colMvCnt; i++ )
        {
            delete[] m_colMvCache[i];
            m_colMvCache[i] = nullptr;
        }
        m_colMvCnt = 0;

        for( int i = 0; i < m_decStateCnt; i++ )
        {
            delete[] m_decStateCache[i];
            m_decStateCache[i] = nullptr;
        }
        m_decStateCnt = 0;
    }

    m_generation++;     // 标记不同的设置
    return true;
}

// 创建一帧 DecFrame, isRef: 是否为参考帧
DecFrame* FrameFactory::create( bool isRef )
{
    irk::Mutex::Guard guard_( m_mutex );
    DecFrame* pFrame = nullptr;

    if( m_frameCnt > 0 )  // 回收之前丢弃的帧
    {
        pFrame = m_frameCache[--m_frameCnt];
        m_frameCache[m_frameCnt] = nullptr;
    }
    else
    {
        // NOTE: 为了增强解码器稳定性, 避免错误码流导致内存越界, 多分配了一些内存
        int frameSize = m_lumaSize + m_chromaSize * 2 + m_lumaPitch * 4 + 64;
        IrkMemBlock mblock = (*m_pfnAlloc)( frameSize, 32, m_allocParam );
        uint8_t* buf = (uint8_t*)mblock.buf + m_lumaPitch * 2 + 32;

        pFrame = (DecFrame*)m_pool.alloc();
        memset( pFrame, 0, sizeof(DecFrame) );
        pFrame->plane[0] = buf;
        pFrame->plane[1] = buf + m_lumaSize;
        pFrame->plane[2] = buf + m_lumaSize + m_chromaSize;
        pFrame->width[0] = m_lumaWidth;
        pFrame->width[1] = m_chromaWidth;
        pFrame->width[2] = m_chromaWidth;
        pFrame->height[0] = m_lumaHeight;
        pFrame->height[1] = m_chromaHeight;
        pFrame->height[2] = m_chromaHeight;
        pFrame->pitch[0] = m_lumaPitch;
        pFrame->pitch[1] = m_chromaPitch;
        pFrame->pitch[2] = m_chromaPitch;
        pFrame->mblock = mblock;
    }

    // 参考帧
    if( isRef )
    {
        assert( pFrame->colMvs == nullptr );
        
        // 分配存储 BDColMvs 的内存
        if( m_colMvCnt > 0 )        // 回收之前丢弃的数据
        {
            pFrame->colMvs = m_colMvCache[--m_colMvCnt];
            m_colMvCache[m_colMvCnt] = nullptr;
        }
        else
        {
            int mbColCnt = XmmPitch( m_lumaWidth ) >> 4;
            int mbRowCnt = YmmPitch( m_lumaHeight ) >> 4;
            pFrame->colMvs = new BDColMvs[mbRowCnt * mbColCnt];
        }
    }

    // 解码状态, 用于并行解码跟踪参考帧状态
    assert( pFrame->decState == nullptr );
    if( m_decStateCnt > 0 )     // 回收之前丢弃的数据
    {
        pFrame->decState = m_decStateCache[--m_decStateCnt];
        m_decStateCache[m_decStateCnt] = nullptr;
        pFrame->decState->reset();
    }
    else
    {
        pFrame->decState = new DecodingState;
    }

    pFrame->refCnt = 1;
    pFrame->generation = m_generation;
    pFrame->factory = this;

    return pFrame;
}

// 丢弃 DecFrame
void FrameFactory::discard( DecFrame* pFrame )
{
    irk::Mutex::Guard guard_( m_mutex );
    assert( pFrame->refCnt == 0 );

    // 释放 BDColMvs
    if( pFrame->colMvs )
    {
        if( m_colMvCnt < kCacheSize && pFrame->generation == m_generation )
            m_colMvCache[m_colMvCnt++] = pFrame->colMvs;
        else
            delete[] pFrame->colMvs;
        pFrame->colMvs = nullptr;
    }

    // 释放解码进度状态
    if( pFrame->decState )
    {
        if( m_decStateCnt < kCacheSize )
            m_decStateCache[m_decStateCnt++] = pFrame->decState;
        else
            delete pFrame->decState;
        pFrame->decState = nullptr;
    }

    // 如果使用缺省内存分配函数, 缓存内存块以便后续复用
    if( m_bDefAlloc && m_frameCnt < kCacheSize && pFrame->generation == m_generation )
    {
        m_frameCache[m_frameCnt++] = pFrame;
    }
    else
    {
        (*m_pfnDealloc)( &pFrame->mblock, m_deallocParam );
        m_pool.dealloc( pFrame );
    }    
}

//======================================================================================================================
FrmDecContext::FrmDecContext()
{
    this->avsCtx = nullptr;
    this->picWidth = 0;
    this->picHeight = 0;
    this->chromaFmt = 0;
    this->mbColCnt = 0;
    this->mbRowCnt = 0;
    this->curFrame = nullptr;
    this->refFrames[0] = nullptr;
    this->refFrames[1] = nullptr;
    this->refDist = this->distBuf + 2;
    this->denDist = this->distBuf + 8;
    this->topMbBuf[0] = nullptr;
    this->topMbBuf[1] = nullptr;

    this->vlcParser = nullptr;
    this->aecParser = nullptr;

    uint8_t* buf = (uint8_t*)irk::aligned_malloc( 4096, 32 );
    this->tempBuf = buf;
    this->mcBuff = buf + 2048;
    this->wqMatrix = buf + 2048 + 1024;
    this->coeff = (int16_t*)(buf + 2048 + 1024 + 256);

    this->decTask = nullptr;
}

FrmDecContext::~FrmDecContext()
{
    assert( this->curFrame == nullptr );
    assert( this->refFrames[0] == nullptr );
    assert( this->refFrames[1] == nullptr );

    if( this->vlcParser )
        delete this->vlcParser;
    if( this->aecParser )
        delete this->aecParser;

    irk::aligned_free( this->topMbBuf[0] );
    irk::aligned_free( this->tempBuf );

    if( this->decTask )
    {
        assert( this->decTask->ref_count() == 1 );
        this->decTask->dismiss();
    }
}

FrmCtxFactory::FrmCtxFactory()
{
    m_ctxCnt = 0;
    memset( m_ctxCache, 0, sizeof(m_ctxCache) );
}

FrmCtxFactory::~FrmCtxFactory()
{
    for( int i = 0; i < m_ctxCnt; i++ )
        delete m_ctxCache[i];
}

// 创建单帧解码 context
FrmDecContext* FrmCtxFactory::create()
{
    FrmDecContext* ctx = nullptr;
    if( m_ctxCnt > 0 )
    {
        ctx = m_ctxCache[--m_ctxCnt];
        m_ctxCache[m_ctxCnt] = nullptr;
    }
    else
    {
        ctx = new FrmDecContext;
    }
    return ctx;
}

// 丢弃单帧解码 context
void FrmCtxFactory::discard( FrmDecContext* ctx )
{
    if( m_ctxCnt < kCacheSize )
    {
        m_ctxCache[m_ctxCnt] = ctx;
        m_ctxCnt++;
    }
    else
    {
        delete ctx;
    }
}

// 创建单帧解码 context
inline FrmDecContext* create_frame_ctx( AvsContext* avsCtx )
{
    FrmDecContext* ctx = avsCtx->ctxFactory.create();
    ctx->avsCtx = avsCtx;
    assert( ctx->curFrame == nullptr );
    assert( ctx->refFrames[0] == nullptr );
    assert( ctx->refFrames[1] == nullptr );
    return ctx;
}

#undef DISMISS_FRAME
#define DISMISS_FRAME(pframe) do{ if( pframe ) { pframe->dismiss(); pframe = nullptr; } }while(0)

inline void discard_frame_ctx( FrmDecContext* ctx )
{
    DISMISS_FRAME( ctx->curFrame );
    DISMISS_FRAME( ctx->refFrames[0] );
    DISMISS_FRAME( ctx->refFrames[1] );
    ctx->avsCtx->ctxFactory.discard( ctx );
}

// 这个简单的类用以保证资源安全
class FrmCtxGuard
{
public:
    explicit FrmCtxGuard( FrmDecContext* ctx ) : m_ctx( ctx ) 
    {}
    ~FrmCtxGuard()  
    { 
        if( m_ctx )
            discard_frame_ctx( m_ctx ); 
    }
    void detach()  
    {
        m_ctx = nullptr; 
    }
private:
    FrmCtxGuard( const FrmCtxGuard& )=delete;
    FrmCtxGuard& operator=( const FrmCtxGuard& )=delete;
    FrmDecContext* m_ctx;
};

//======================================================================================================================

// 查找 start code, 返回 start code 所在位置, 找不到返回输入数据长度
static int find_start_code( uint8_t* data, int size )
{
    uint32_t sc32 = 0xFFFFFFFF;
    for( int i = 0; i < size; i++ )
    {
        sc32 = (sc32 << 8) | data[i];
        if( sc32 <= 0x1FF )     // 找到 start code
        {
            assert( i >= 3 );
            return i - 3;
        }
    }
    return size;
};

// 查找下一个 start code, 返回下一个 start code 所在位置, 找不到返回输入数据长度
static int find_next_start_code( uint8_t* data, int size )
{
    assert( size > 4 );
    assert( (*(uint32_t*)data & 0xFFFFFF) == 0x010000 );

    uint32_t sc32 = 0xFFFFFFFF;
    for( int i = 4; i < size; i++ )
    {
        sc32 = (sc32 << 8) | data[i];
        if( (sc32 & 0xFFFFFF) == 0x1 )  // 找到下一个 start code
        {
            assert( i >= 6 );
            return i - 2;
        }
    }
    return size;
};

// 查找下一个 picture, 返回所在位置, 找不到返回输入数据长度
static int find_next_picture( uint8_t* data, int size )
{
    assert( size > 4 );
    assert( (*(uint32_t*)data & 0xFFFFFF) == 0x010000 );

    uint32_t sc32 = 0xFFFFFFFF;
    for( int i = 4; i < size; i++ )
    {
        sc32 = (sc32 << 8) | data[i];
        if( sc32 <= 0x1FF )     // 找到 start code
        {
            // 找到序列头, 图像头, 视频编辑码
            if( sc32 == 0x1B0 || sc32 == 0x1B3 || sc32 == 0x1B6 || sc32 == 0x1B7 )
                return i - 3;
        }
    }
    return size;
};

// 去除 AVS 伪起始码
// 返回已处理的原始数据长度, *plen 赋值为处理后的数据长度
static int remove_faked_start_code( uint8_t* data, int size, int* pLen )
{
    assert( data[0] == 0x2 && size > 0 );
    AvsBitWriter bitsw;
    bitsw.setup( data );    // 初始化, 写出 6 个比特的 0

    uint32_t sc32 = 0xFFFFFFFF;
    for( int i = 1; i < size; i++ )
    {
        sc32 = (sc32 << 8) | data[i];
        sc32 &= 0xFFFFFF;
        if( sc32 > 0x2 )            // 正常数据
        {
            bitsw.write_byte( data[i] );
        }
        else if( sc32 == 0x1 )      // 下一个起始码
        {
            assert( i >= 3 );
            bitsw.make_byte_aligned();
            *pLen = bitsw.get_size() - 2;
            return i - 2;
        }
        else if( sc32 == 0x2 )      // 伪起始码
        {
            bitsw.write_6bits( data[i] );
        }
        else
        {
            bitsw.write_byte( 0 );  // 实为码流错误, 为了增强容错性, 无视
        }
    }

    bitsw.make_byte_aligned();
    *pLen = bitsw.get_size();
    return size;
}

// 查找下一个 start code 并去除当前 start code unit 里面的伪起始码
// 返回当前 start code unit 原始数据长度, *pLen 为处理后的 start code unit 长度
static int split_and_purify_sc_unit( uint8_t* data, int size, int* pLen )
{
    assert( size > 4 );
    assert( (*(uint32_t*)data & 0xFFFFFF) == 0x010000 );

    uint32_t sc32 = 0xFFFFFFFF;
    for( int i = 4; i < size; i++ )
    {
        sc32 = (sc32 << 8) | data[i];
        if( (sc32 & 0xFFFFFF) <= 0x2 )
        {
            if( data[i] == 0x1 )        // 找到下一个 start code
            {
                *pLen = i - 2;
                return i - 2;
            }
            else if( data[i] == 0x2 )   // 伪起始码
            {
                int lens = 0;
                int used = remove_faked_start_code( data + i, size - i, &lens );
                assert( lens > 0 && used >= lens );
                lens += i;
                used += i;

                // 填充 0 有助于解码时数据末尾检测               
                for( int j = lens; j < used; j++ )
                    data[j] = 0;
                *pLen = lens;
                return used;
            }
            else
            {
                // 实为码流错误, 为了增强容错性, 忽略
            }
        }
    }

    // 到达数据末尾
    *pLen = size;
    return size;
}

// 解析sequence header 并检测是否正确, 解码器是否支持
static int parse_and_check_seqhdr( AvsSeqHdr* hdr, const uint8_t* data, int size )
{
    // 解析 sequence header
    if( !parse_seq_header( hdr, data, size ) )
        return IRK_AVS_DEC_BAD_STREAM;

    // 本解码器支持基准和广播 profile
    if( hdr->profile != 0x20 && hdr->profile != 0x48 )
        return IRK_AVS_DEC_UNSUPPORTED;

    // 分辨率检查, 最大支持的分辨率为 4096x2160, 最小分辨率为 176x144
    if( hdr->width > 4096 || hdr->width < 176 )
        return IRK_AVS_DEC_UNSUPPORTED;
    if( hdr->height > 2160 || hdr->height < 144 )
        return IRK_AVS_DEC_UNSUPPORTED;
    if( hdr->height & 1 )
        return IRK_AVS_DEC_BAD_STREAM;

    // 8 bit, 420
    if( hdr->sample_precision != 1 )
        return IRK_AVS_DEC_UNSUPPORTED;
    if( hdr->chroma_format != AVS_CHROMA_420 )
        return IRK_AVS_DEC_UNSUPPORTED;

    return 0;
}

static int begin_frame_decoding( FrmDecContext* );
static void end_frame_decoding( FrmDecContext* );

// 等待最前面的一帧视频解码完成, 需要在主线程调用
static void wait_one_frame( AvsContext* avsCtx )
{
    assert( !avsCtx->workingQueue.is_empty() );

    FrmDecContext* frmCtx = nullptr;
    avsCtx->workingQueue.pop_front( &frmCtx );
    assert( frmCtx );

    frmCtx->curFrame->decState->wait_frame_done();
    end_frame_decoding( frmCtx );
    discard_frame_ctx( frmCtx );
}

// 等待所有视频帧解码完成, 需要在主线程调用
static void wait_all_frames( AvsContext* avsCtx )
{
    assert( !avsCtx->workingQueue.is_empty() );

    FrmDecContext* frmCtx = nullptr;
    while( avsCtx->workingQueue.pop_front( &frmCtx ) )
    {
        frmCtx->curFrame->decState->wait_frame_done();
        end_frame_decoding( frmCtx );
        discard_frame_ctx( frmCtx );
    }
    assert( avsCtx->workingQueue.is_empty() );
}

// 结束当前视频序列, 清理参考帧, 需要在主线程调用
static void end_sequence_decoding( AvsContext* ctx )
{
    // 等待之前的所有的异步解码线程完成
    if( !ctx->workingQueue.is_empty() )
    {
        wait_all_frames( ctx );
    }

    // 输出之前序列缓存的帧
    if( ctx->outFrame )
    {
        (*ctx->pfnNotify)( IRK_CODEC_DONE, ctx->outFrame, ctx->notifyParam );
        ctx->outFrame->dismiss();
        ctx->outFrame = nullptr;
    }

    // 丢弃存储的参考帧
    DISMISS_FRAME( ctx->refFrames[0] );
    DISMISS_FRAME( ctx->refFrames[1] );

    // 标记需要新的 sequence header
    ctx->status = 0;
}

// 开始新的视频序列, 需要在主线程调用
static int begin_sequence_decoding( AvsContext* ctx, const uint8_t* data, int size )
{
    assert( *(uint32_t*)data == 0xB0010000 );
    AvsSeqHdr& seqHdr = ctx->seqHdr;

    if( ctx->status == 0 )  // 初始状态, sequence header 未解析
    {
        assert( ctx->workingQueue.is_empty() );
        assert( ctx->refFrames[0] == nullptr && ctx->refFrames[1] == nullptr );
        assert( ctx->outFrame == nullptr );

        // 解析 sequence header 并检测是否正确, 解码器是否支持
        int errc = parse_and_check_seqhdr( &seqHdr, data, size );
        if( errc != 0 )
            return errc;

        // 配置视频帧缓存
        ctx->frmFactory.config( seqHdr.width, seqHdr.height, seqHdr.chroma_format );
    }
    else    // sequence header 已解析, 查看是否发生变化
    {
        // 解析新的 sequence header 并检测是否正确, 解码器是否支持
        AvsSeqHdr newHdr;
        int errc = parse_and_check_seqhdr( &newHdr, data, size );
        if( errc != 0 )
        {
            // 解析失败, 结束当前序列解码, 等待正确的 sequence header
            end_sequence_decoding( ctx );
            return errc;
        }

        // 查看视频分辨率, 编码方式等是否发生了变化
        if( newHdr.profile != seqHdr.profile ||
            newHdr.width != seqHdr.width || newHdr.height != seqHdr.height ||
            newHdr.chroma_format != seqHdr.chroma_format )
        {
            // sequence header 发生变化, 结束当前序列解码
            end_sequence_decoding( ctx );

            // 重新配置视频帧缓存
            ctx->frmFactory.config( newHdr.width, newHdr.height, newHdr.chroma_format );
        }

        // 复制新的 sequence header
        seqHdr = newHdr;
    }

    // 内部使用的视频大小为宏块的整数倍
    ctx->frameWidth = XmmPitch( seqHdr.width );
    ctx->frameHeight = XmmPitch( seqHdr.height );
    ctx->status = AVS_SEQ_HDR_PARSED;
    return 0;
}

// 开始新一帧的解码, 设置相关参数, 需要在主线程调用
static int begin_frame_decoding( FrmDecContext* frmCtx )
{
    AvsContext* avsCtx = frmCtx->avsCtx;
    const AvsPicHdr& picHdr = frmCtx->picHdr;
    
    // 创建解码后的视频帧
    const bool isRef = picHdr.pic_type != PIC_TYPE_B;   // I,P 帧为参考帧
    DecFrame* curFrame = avsCtx->frmFactory.create( isRef );
    curFrame->userpts            = frmCtx->userPts;
    curFrame->userdata           = frmCtx->userData;
    curFrame->pic_type           = picHdr.pic_type;
    curFrame->progressive        = picHdr.progressive_frame;
    curFrame->topfield_first     = picHdr.top_field_first;
    curFrame->repeat_first_field = picHdr.repeat_first_field;
    curFrame->frameCoding        = picHdr.picture_structure;
    curFrame->poc                = picHdr.pic_distance * 2;
    assert( frmCtx->curFrame == nullptr );
    frmCtx->curFrame = curFrame;

    // 分配解码使用的内部空间
    if( frmCtx->picWidth != avsCtx->frameWidth )
    {
        if( frmCtx->topMbBuf[0] )
        {
            irk::aligned_free( frmCtx->topMbBuf[0] );
            frmCtx->topMbBuf[0] = nullptr;
        }
        const int colCnt = (avsCtx->frameWidth >> 4) + 2;
        frmCtx->topMbBuf[0] = (MbContext*)irk::aligned_malloc( sizeof(MbContext) * 2 * colCnt, 16 );
        frmCtx->topMbBuf[1] = frmCtx->topMbBuf[0] + colCnt;
    }

    // 根据帧场类型设置全局解码参数
    frmCtx->pFieldSkip = 0;
    frmCtx->bFieldEnhance = 0;
    if( picHdr.picture_structure != 0 ) // 帧编码
    {   
        frmCtx->picWidth = avsCtx->frameWidth;
        frmCtx->picHeight = avsCtx->frameHeight;      
        frmCtx->invScan = s_InvScan;
        frmCtx->picPlane[0] = curFrame->plane[0];
        frmCtx->picPlane[1] = curFrame->plane[1];
        frmCtx->picPlane[2] = curFrame->plane[2];
        frmCtx->picPitch[0] = curFrame->pitch[0];
        frmCtx->picPitch[1] = curFrame->pitch[1];
        frmCtx->picPitch[2] = curFrame->pitch[2];
    }
    else    // 场编码
    {
        if( avsCtx->seqHdr.profile == 0x48 && picHdr.pic_type == PIC_TYPE_P )
            frmCtx->pFieldSkip = picHdr.pb_field_enhanced_flag;
        if( avsCtx->seqHdr.profile == 0x48 && picHdr.pic_type == PIC_TYPE_B )
            frmCtx->bFieldEnhance = picHdr.pb_field_enhanced_flag;

        frmCtx->picWidth = avsCtx->frameWidth;
        frmCtx->picHeight = XmmPitch( avsCtx->frameHeight >> 1 );
        frmCtx->invScan = s_InvScan + 64;

        if( picHdr.top_field_first )    // 顶场在前
        {
            frmCtx->picPlane[0] = curFrame->plane[0];
            frmCtx->picPlane[1] = curFrame->plane[1];
            frmCtx->picPlane[2] = curFrame->plane[2];
        }
        else    // 底场在前
        {
            frmCtx->picPlane[0] = curFrame->plane[0] + curFrame->pitch[0];
            frmCtx->picPlane[1] = curFrame->plane[1] + curFrame->pitch[1];
            frmCtx->picPlane[2] = curFrame->plane[2] + curFrame->pitch[2];
        }
        frmCtx->picPitch[0] = curFrame->pitch[0] * 2;
        frmCtx->picPitch[1] = curFrame->pitch[1] * 2;
        frmCtx->picPitch[2] = curFrame->pitch[2] * 2;
    }

    // 色差宽度和高度
    frmCtx->chromaFmt = avsCtx->seqHdr.chroma_format;
    if( frmCtx->chromaFmt == AVS_CHROMA_420 )
    {
        frmCtx->chromaWidth = frmCtx->picWidth >> 1;
        frmCtx->chromaHeight = frmCtx->picHeight >> 1;
    }
    else
    {
        return IRK_AVS_DEC_UNSUPPORTED;
    }

    frmCtx->mbColCnt = frmCtx->picWidth >> 4;
    frmCtx->mbRowCnt = frmCtx->picHeight >> 4;
    frmCtx->frameCoding = picHdr.picture_structure;
    frmCtx->fieldIdx = 0;

    // 设置有效参考范围, 左右边界有 8 字节填充
    frmCtx->refRcLuma.x1 = -8;
    frmCtx->refRcLuma.y1 = -1;
    frmCtx->refRcLuma.x2 = frmCtx->picWidth + 8;
    frmCtx->refRcLuma.y2 = frmCtx->picHeight;
    frmCtx->refRcCbcr.x1 = -8;
    frmCtx->refRcCbcr.y1 = -1;
    frmCtx->refRcCbcr.x2 = frmCtx->chromaWidth + 8;
    frmCtx->refRcCbcr.y2 = frmCtx->chromaHeight;

    // 设置 weight quant matrix
    if( picHdr.weight_quant_flag == 0 ) // 不使用加权量化
    {
        memset( frmCtx->wqMatrix, 128, 64 );
    }
    else
    {
        alignas(uint64_t) uint8_t wqP[8];
        *(uint64_as*)wqP = *(uint64_as*)s_WQParam[picHdr.weight_quant_index];
        if( picHdr.weight_quant_index != 0 )
        {
            wqP[0] = (uint8_t)(wqP[0] + picHdr.weight_quant_param_delta[0]);
            wqP[1] = (uint8_t)(wqP[1] + picHdr.weight_quant_param_delta[1]);
            wqP[2] = (uint8_t)(wqP[2] + picHdr.weight_quant_param_delta[2]);
            wqP[3] = (uint8_t)(wqP[3] + picHdr.weight_quant_param_delta[3]);
            wqP[4] = (uint8_t)(wqP[4] + picHdr.weight_quant_param_delta[4]);
            wqP[5] = (uint8_t)(wqP[5] + picHdr.weight_quant_param_delta[5]);
        }
        if( picHdr.weight_quant_model == 0 )        // CurrentSceneModel == 0
        {
            uint8_t temp[64] = WQ_MODEL_0( wqP );
            memcpy( frmCtx->wqMatrix, temp, 64 * sizeof(uint8_t) );
        }
        else if( picHdr.weight_quant_model == 1 )   // CurrentSceneModel == 1
        {
            uint8_t temp[64] = WQ_MODEL_1( wqP );
            memcpy( frmCtx->wqMatrix, temp, 64 * sizeof(uint8_t) );
        }
        else    // CurrentSceneModel == 2
        {
            uint8_t temp[64] = WQ_MODEL_2( wqP );
            memcpy( frmCtx->wqMatrix, temp, 64 * sizeof(uint8_t) );
        }
    }

    // 码流本身或者用户禁用环路滤波
    frmCtx->lfDisabled = picHdr.loop_filter_disable | avsCtx->config.disable_lf;

    if( picHdr.aec_enable ) // 高级熵编码
    {
        if( frmCtx->aecParser == nullptr )
            frmCtx->aecParser = new AvsAecParser;
        frmCtx->aecParser->set_scan_quant_matrix( frmCtx->invScan, frmCtx->wqMatrix );

        // AVS+ 高级熵编码的设计, 如果码流错误, 存在大量的连续 0 可能导致内存越界
        uint8_t* picData = frmCtx->dataBuf.data();
        size_t picSize = frmCtx->dataBuf.size();
        assert( frmCtx->dataBuf.capacity() >= picSize + 4096 );
        memset( picData + picSize + 8, 0xFF, 4096 - 8 );
    }
    else // VLC 编码
    {
        if( frmCtx->vlcParser == nullptr )
            frmCtx->vlcParser = new AvsVlcParser;
        frmCtx->vlcParser->set_scan_quant_matrix( frmCtx->invScan, frmCtx->wqMatrix );
    }

    return 0;
}

// 结束一帧的解码, 输出解码后的视频帧给用户, 需要在主线程调用
static void end_frame_decoding( FrmDecContext* frmCtx )
{
    AvsContext* avsCtx = frmCtx->avsCtx;

    if( frmCtx->picHdr.pic_type == PIC_TYPE_B || avsCtx->config.output_order != 0  || avsCtx->seqHdr.low_delay )
    {
        // B 帧或者用户要求按编码顺序输出, 立即输出当前帧
        (*avsCtx->pfnNotify)( IRK_CODEC_DONE, frmCtx->curFrame, avsCtx->notifyParam );
        frmCtx->curFrame->dismiss();
        frmCtx->curFrame = nullptr;
    }
    else
    {
        if( avsCtx->outFrame )  // 输出上一个参考帧
        {
            (*avsCtx->pfnNotify)( IRK_CODEC_DONE, avsCtx->outFrame, avsCtx->notifyParam );
            avsCtx->outFrame->dismiss();
            avsCtx->outFrame = nullptr;
        }
        avsCtx->outFrame = frmCtx->curFrame;
        frmCtx->curFrame = nullptr;
    }
}

// 准备当前帧需要的参考帧, 需要在主线程调用
static void prepare_ref_frames( FrmDecContext* frmCtx )
{
    AvsContext* avsCtx = frmCtx->avsCtx;
    assert( frmCtx->curFrame );
    assert( frmCtx->refFrames[0] == nullptr && frmCtx->refFrames[1] == nullptr );

    // 最近的参考帧
    if( avsCtx->refFrames[0] )
    {
        frmCtx->refFrames[0] = avsCtx->refFrames[0];
        frmCtx->refFrames[0]->add_ref();
    }
    else
    {
        // 参考帧不存在, 当前帧实际无法解码, 为增强容错性, 伪造一个参考帧
        DecFrame* fakedRef = avsCtx->frmFactory.create( true );
        fakedRef->progressive = frmCtx->curFrame->progressive;
        fakedRef->topfield_first = frmCtx->curFrame->topfield_first;
        if( frmCtx->picHdr.pic_type == PIC_TYPE_P )
            fakedRef->poc = frmCtx->curFrame->poc - 2;
        else
            fakedRef->poc = frmCtx->curFrame->poc + 2;

        // B_Direct 帧间预测参数
        if( frmCtx->picHdr.pic_type == PIC_TYPE_B )
        {        
            int mbCnt = (avsCtx->frameWidth >> 4) * (avsCtx->frameHeight >> 4);
            memset( fakedRef->colMvs, 0, sizeof(BDColMvs) * mbCnt );
        }

        fakedRef->decState->set_frame_done();
        frmCtx->refFrames[0] = fakedRef;
    }

    // 第二个参考帧
    if( avsCtx->refFrames[1] )
    {
        frmCtx->refFrames[1] = avsCtx->refFrames[1];
        frmCtx->refFrames[1]->add_ref();
    }
    else
    {
        // 参考帧不存在, 当前帧实际无法解码, 为增强容错性, 伪造一个参考帧
        DecFrame* fakedRef = avsCtx->frmFactory.create( true );
        fakedRef->progressive = frmCtx->curFrame->progressive;
        fakedRef->topfield_first = frmCtx->curFrame->topfield_first;
        if( frmCtx->picHdr.pic_type == PIC_TYPE_P )
            fakedRef->poc = frmCtx->refFrames[0]->poc - 2;
        else
            fakedRef->poc = frmCtx->curFrame->poc - 2;

        fakedRef->decState->set_frame_done();
        frmCtx->refFrames[1] = fakedRef;
    }       
}

//======================================================================================================================
extern void decode_slice_I( FrmDecContext* ctx, const uint8_t* data, int size );
extern void decode_slice_P( FrmDecContext* ctx, const uint8_t* data, int size );
extern void decode_slice_B( FrmDecContext* ctx, const uint8_t* data, int size );
extern void decode_slice_I_AEC( FrmDecContext* ctx, const uint8_t* data, int size );
extern void decode_slice_P_AEC( FrmDecContext* ctx, const uint8_t* data, int size );
extern void decode_slice_B_AEC( FrmDecContext* ctx, const uint8_t* data, int size );

#define COPY3X( dst, src ) do{ \
    (dst)[0] = (src)[0]; \
    (dst)[1] = (src)[1]; \
    (dst)[2] = (src)[2]; }while(0)

#define ADD3X( dst, src1, src2 ) do{    \
    (dst)[0] = (src1)[0] + (src2)[0];   \
    (dst)[1] = (src1)[1] + (src2)[1];   \
    (dst)[2] = (src1)[2] + (src2)[2]; }while(0)

// I 帧解码,  可能在后台线程调用
static void decode_frame_I( FrmDecContext* ctx )
{
    // 帧或者第一场
    ctx->maxRefIdx = 0;
    ctx->fieldIdx = 0;

    // 针对后续 B 帧的 B_Direct 运动估计
    assert( ctx->curFrame );
    DecFrame* curFrame = ctx->curFrame;
    curFrame->denDistBD[0][0] = 0;
    curFrame->denDistBD[0][1] = 0;
    for( int i = 0; i < ctx->mbRowCnt * ctx->mbColCnt; i++ )
        *(uint32_as*)(curFrame->colMvs[i].refIdxs) = 0xFFFFFFFF;
    ctx->colMvs = nullptr;
    
    // slice 解码函数
    PFN_DecodeSlice pfn_DecSlice = ctx->picHdr.aec_enable ? &decode_slice_I_AEC : &decode_slice_I;

    // slice 逐一解码
    const SliceVector& sliVec = ctx->sliceVec;
    int sIdx = 0;
    for( ; sIdx < sliVec.size(); sIdx++ )
    {
        const uint8_t* data = sliVec[sIdx].data;
        if( ctx->frameCoding == 0 && data[3] >= ctx->mbRowCnt ) // 第二场
            break;  
        (*pfn_DecSlice)( ctx, data, sliVec[sIdx].size );
    }

    // 帧编码或者未检测到第二场数据, 解码结束
    if( sIdx >= sliVec.size() )
        return;

    //------------------------------------------------------------------------------------   
    ctx->fieldIdx = 1;      // 第二场

    // 设置参考场, 当前帧第一场
    ctx->refPics[0].pframe = curFrame;
    COPY3X( ctx->refPics[0].plane, ctx->picPlane );

    // 设置参考帧距离
    ctx->refDist[-1] = 1;
    ctx->refDist[0] = 1;
    ctx->refDist[1] = 0;
    ctx->denDist[-1] = 512;
    ctx->denDist[0] = 512;
    ctx->denDist[1] = 0;

    // 针对后续 B 帧的 B_Direct 运动估计
    ctx->colMvs = curFrame->colMvs + ctx->mbRowCnt * ctx->mbColCnt;
    curFrame->denDistBD[1][0] = 16384;
    curFrame->denDistBD[1][1] = 0;

    // 第二场在帧内的位置
    if( ctx->picHdr.top_field_first )
    {
        ADD3X( ctx->picPlane, curFrame->plane, curFrame->pitch );
    }
    else
    {
        COPY3X( ctx->picPlane, curFrame->plane );
    }

    // 场编码下 I 帧的第二场是 P 场
    pfn_DecSlice = ctx->picHdr.aec_enable ? &decode_slice_P_AEC : &decode_slice_P;

    // slice 逐一解码
    for( ; sIdx < sliVec.size(); sIdx++ )
    {
        (*pfn_DecSlice)( ctx, sliVec[sIdx].data, sliVec[sIdx].size );
    }
}

// P 帧解码, 可能在后台线程调用
static void decode_frame_P( FrmDecContext* ctx )
{
    DecFrame* curFrame = ctx->curFrame;
    DecFrame** refFrames = ctx->refFrames;
    RefPicture* refPics = ctx->refPics;
    const int* framePitchs = curFrame->pitch;
    
    if( ctx->frameCoding )  // 帧编码
    {
        ctx->maxRefIdx = 1;
        ctx->fieldIdx = 0;

        // 帧编码有 2 个参考帧
        refPics[0].pframe = refFrames[0];
        refPics[1].pframe = refFrames[1];
        COPY3X( refPics[0].plane, refFrames[0]->plane );
        COPY3X( refPics[1].plane, refFrames[1]->plane );

        // 设置参考帧距离
        ctx->refDist[-1] = 1;
        ctx->refDist[0] = (curFrame->poc - refFrames[0]->poc + 512) & 511;
        ctx->refDist[1] = (curFrame->poc - refFrames[1]->poc + 512) & 511;
        if( ctx->refDist[0] == 0 )      // 实为码流错误, 为增强容错性忽略之
            ctx->refDist[0] = 2;
        if( ctx->refDist[1] == 0 )
            ctx->refDist[1] = 4;
        ctx->denDist[-1] = 512;
        ctx->denDist[0] = 512 / ctx->refDist[0];
        ctx->denDist[1] = 512 / ctx->refDist[1];

        // 针对 B_Direct 运动估计
        curFrame->denDistBD[0][0] = 16384 / ctx->refDist[0];
        curFrame->denDistBD[0][1] = 16384 / ctx->refDist[1];
    }
    else    // 场编码
    {
        ctx->maxRefIdx = 3;
        ctx->fieldIdx = 0;      // 第一场

        // 场编码有 4 个参考帧
        refPics[0].pframe = refFrames[0];
        refPics[1].pframe = refFrames[0];
        refPics[2].pframe = refFrames[1];
        refPics[3].pframe = refFrames[1];
        if( refFrames[0]->topfield_first )
        {
            ADD3X( refPics[0].plane, refFrames[0]->plane, framePitchs );
            COPY3X( refPics[1].plane, refFrames[0]->plane );
        }
        else
        {
            COPY3X( refPics[0].plane, refFrames[0]->plane );
            ADD3X( refPics[1].plane, refFrames[0]->plane, framePitchs );          
        }
        if( refFrames[1]->topfield_first )
        {
            ADD3X( refPics[2].plane, refFrames[1]->plane, framePitchs );
            COPY3X( refPics[3].plane, refFrames[1]->plane );
        }
        else
        {
            COPY3X( refPics[2].plane, refFrames[1]->plane );
            ADD3X( refPics[3].plane, refFrames[1]->plane, framePitchs );  
        }      

        // 设置参考帧距离
        ctx->refDist[-1] = 1;
        ctx->refDist[0] = (curFrame->poc - refFrames[0]->poc + 511) & 511;
        ctx->refDist[1] = (curFrame->poc - refFrames[0]->poc + 512) & 511;
        ctx->refDist[2] = (curFrame->poc - refFrames[1]->poc + 511) & 511;
        ctx->refDist[3] = (curFrame->poc - refFrames[1]->poc + 512) & 511;
        if( ctx->refDist[0] == 0 )  // 实为码流错误, 为增强容错性忽略之
            ctx->refDist[0] = 1;
        if( ctx->refDist[1] == 0 )
            ctx->refDist[1] = 2;
        if( ctx->refDist[2] == 0 )
            ctx->refDist[2] = 3;
        if( ctx->refDist[3] == 0 )
            ctx->refDist[3] = 4;
        ctx->denDist[-1] = 512;
        ctx->denDist[0] = 512 / ctx->refDist[0];
        ctx->denDist[1] = 512 / ctx->refDist[1];
        ctx->denDist[2] = 512 / ctx->refDist[2];
        ctx->denDist[3] = 512 / ctx->refDist[3];

        // 针对 B_Direct 运动估计
        curFrame->denDistBD[0][0] = 16384 / ctx->refDist[0];
        curFrame->denDistBD[0][1] = 16384 / ctx->refDist[1];
        curFrame->denDistBD[0][2] = 16384 / ctx->refDist[2];
        curFrame->denDistBD[0][3] = 16384 / ctx->refDist[3];
    }

    // 针对 B_Direct 运动估计
    ctx->colMvs = curFrame->colMvs;

    // slice 解码函数
    PFN_DecodeSlice pfn_DecSlice = ctx->picHdr.aec_enable ? &decode_slice_P_AEC : &decode_slice_P;

    // slice 逐一解码
    const SliceVector& sliVec = ctx->sliceVec;
    int sIdx = 0;
    for( ; sIdx < sliVec.size(); sIdx++ )
    {
        const uint8_t* data = sliVec[sIdx].data;
        if( ctx->frameCoding == 0 && data[3] >= ctx->mbRowCnt ) // 第二场
            break;  
        (*pfn_DecSlice)( ctx, data, sliVec[sIdx].size );
    }

    // 帧编码或者未检测到第二场数据, 解码结束
    if( sIdx >= sliVec.size() )
        return;

    //------------------------------------------------------------------------------------
    ctx->fieldIdx = 1;      // 第二场

    // 第一个参考场为当前帧第一场
    refPics[0].pframe = curFrame;
    refPics[1].pframe = refFrames[0];
    refPics[2].pframe = refFrames[0];
    refPics[3].pframe = refFrames[1];
    COPY3X( refPics[0].plane, ctx->picPlane );

    if( refFrames[0]->topfield_first )
    {
        ADD3X( refPics[1].plane, refFrames[0]->plane, framePitchs );
        COPY3X( refPics[2].plane, refFrames[0]->plane );
    }
    else
    {
        COPY3X( refPics[1].plane, refFrames[0]->plane );
        ADD3X( refPics[2].plane, refFrames[0]->plane, framePitchs );          
    }
    if( refFrames[1]->topfield_first )
    {
        ADD3X( refPics[3].plane, refFrames[1]->plane, framePitchs );
    }
    else
    {
        COPY3X( refPics[3].plane, refFrames[1]->plane );       
    }

    // 设置参考场距离
    ctx->refDist[-1] = 1;
    ctx->refDist[0] = 1;
    ctx->refDist[1] = (curFrame->poc - refFrames[0]->poc + 512) & 511;
    ctx->refDist[2] = (curFrame->poc - refFrames[0]->poc + 513) & 511;
    ctx->refDist[3] = (curFrame->poc - refFrames[1]->poc + 512) & 511;
    if( ctx->refDist[1] == 0 )  // 实为码流错误, 为增强容错性忽略之
        ctx->refDist[1] = 2;
    if( ctx->refDist[2] == 0 )
        ctx->refDist[2] = 3;
    if( ctx->refDist[3] == 0 )
        ctx->refDist[3] = 4;
    ctx->denDist[-1] = 512;
    ctx->denDist[0] = 512 / ctx->refDist[0];
    ctx->denDist[1] = 512 / ctx->refDist[1];
    ctx->denDist[2] = 512 / ctx->refDist[2];
    ctx->denDist[3] = 512 / ctx->refDist[3];

    // 针对 B_Direct 运动估计
    curFrame->denDistBD[1][0] = 16384 / ctx->refDist[0];
    curFrame->denDistBD[1][1] = 16384 / ctx->refDist[1];
    curFrame->denDistBD[1][2] = 16384 / ctx->refDist[2];
    curFrame->denDistBD[1][3] = 16384 / ctx->refDist[3];

    // 第二场在帧内的位置
    if( ctx->picHdr.top_field_first )
    {
        ADD3X( ctx->picPlane, curFrame->plane, framePitchs );
    }
    else
    {
        COPY3X( ctx->picPlane, curFrame->plane );
    }

    // 针对 B_Direct 运动估计
    ctx->colMvs = curFrame->colMvs + ctx->mbRowCnt * ctx->mbColCnt;

    // slice 逐一解码
    for( ; sIdx < sliVec.size(); sIdx++ )
    {
        (*pfn_DecSlice)( ctx, sliVec[sIdx].data, sliVec[sIdx].size );
    }
}

// B 帧解码, 可能在后台线程调用
static void decode_frame_B( FrmDecContext* ctx )
{
    DecFrame* curFrame = ctx->curFrame;
    DecFrame** refFrames = ctx->refFrames;
    RefPicture* refPics = ctx->refPics;
    const int* framePitchs = curFrame->pitch;

    if( ctx->frameCoding )  // 帧编码
    {
        ctx->maxRefIdx = 0;
        ctx->fieldIdx = 0;
        ctx->backIdxXor = 0x1;

        // 帧编码有 2 个参考帧
        refPics[0].pframe = refFrames[1];
        refPics[1].pframe = refFrames[0];
        COPY3X( refPics[0].plane, refFrames[1]->plane );
        COPY3X( refPics[1].plane, refFrames[0]->plane );
    
        // 设置参考帧距离
        ctx->refDist[-1] = 1;
        ctx->refDist[0] = (curFrame->poc - refFrames[1]->poc + 512) & 511;
        ctx->refDist[1] = (refFrames[0]->poc - curFrame->poc + 512) & 511;
        if( ctx->refDist[0] == 0 )       // 实为码流错误, 为增强容错性忽略之
            ctx->refDist[0] = 2;
        if( ctx->refDist[1] == 0 )
            ctx->refDist[1] = 2;
        ctx->denDist[-1] = 512;
        ctx->denDist[0] = 512 / ctx->refDist[0];
        ctx->denDist[1] = 512 / ctx->refDist[1];
        ctx->backMvScale[0] = ctx->refDist[1] * ctx->denDist[0];
        ctx->backMvScale[1] = ctx->backMvScale[2] = ctx->backMvScale[3] = 0;
    }
    else    // 场编码
    {
        ctx->maxRefIdx = 1;
        ctx->fieldIdx = 0;          // 第一场
        ctx->backIdxXor = 0x3;

        // 标准前向参考索引 0 对应 0, 1 对应 2
        refPics[0].pframe = refFrames[1];
        refPics[2].pframe = refFrames[1];
        refPics[1].pframe = refFrames[0];
        refPics[3].pframe = refFrames[0];
        if( refFrames[1]->topfield_first )
        {
            ADD3X( refPics[0].plane, refFrames[1]->plane, framePitchs );
            COPY3X( refPics[2].plane, refFrames[1]->plane );    
        }
        else
        {
            COPY3X( refPics[0].plane, refFrames[1]->plane );
            ADD3X( refPics[2].plane, refFrames[1]->plane, framePitchs );          
        }

        // 标准后向参考索引 0 对应 1, 1 对应 3
        if( refFrames[0]->topfield_first )
        {
            COPY3X( refPics[1].plane, refFrames[0]->plane );
            ADD3X( refPics[3].plane, refFrames[0]->plane, framePitchs );         
        }
        else
        {
            ADD3X( refPics[1].plane, refFrames[0]->plane, framePitchs );
            COPY3X( refPics[3].plane, refFrames[0]->plane );        
        }

        // 设置参考帧距离
        ctx->refDist[-1] = 1;
        ctx->refDist[0] = (curFrame->poc - refFrames[1]->poc + 511) & 511;
        ctx->refDist[2] = (curFrame->poc - refFrames[1]->poc + 512) & 511;
        ctx->refDist[1] = (refFrames[0]->poc - curFrame->poc + 512) & 511;
        ctx->refDist[3] = (refFrames[0]->poc - curFrame->poc + 513) & 511;  
        if( ctx->refDist[0] == 0 )      // 实为码流错误, 为增强容错性忽略之
            ctx->refDist[0] = 1;
        if( ctx->refDist[2] == 0 )
            ctx->refDist[2] = 2;
        if( ctx->refDist[1] == 0 )
            ctx->refDist[1] = 2; 
        if( ctx->refDist[3] == 0 )
            ctx->refDist[3] = 3;
        ctx->denDist[-1] = 512;
        ctx->denDist[0] = 512 / ctx->refDist[0];
        ctx->denDist[1] = 512 / ctx->refDist[1];
        ctx->denDist[2] = 512 / ctx->refDist[2];
        ctx->denDist[3] = 512 / ctx->refDist[3];
        ctx->backMvScale[0] = ctx->refDist[3] * ctx->denDist[0];
        ctx->backMvScale[1] = 0;
        ctx->backMvScale[2] = ctx->refDist[1] * ctx->denDist[2];
        ctx->backMvScale[3] = 0;
    }

    // 非参考帧, 无需管后续 B 帧的 Direct 运动估计
    ctx->colMvs = nullptr;

    // slice 解码函数
    PFN_DecodeSlice pfn_DecSlice = ctx->picHdr.aec_enable ? &decode_slice_B_AEC : &decode_slice_B;

    // slice 逐一解码
    const SliceVector& sliVec = ctx->sliceVec;
    int sIdx = 0;
    for( ; sIdx < sliVec.size(); sIdx++ )
    {
        const uint8_t* data = sliVec[sIdx].data;
        if( ctx->frameCoding == 0 && data[3] >= ctx->mbRowCnt ) // 第二场
            break;  
        (*pfn_DecSlice)( ctx, data, sliVec[sIdx].size );
    }

    // 帧编码或者未检测到第二场数据, 解码结束
    if( sIdx >= sliVec.size() )
        return;

    //------------------------------------------------------------------------------------
    ctx->fieldIdx = 1;          // 第二场
    ctx->backIdxXor = 0x3;

    // 设置参考场距离
    ctx->refDist[-1] = 1;
    ctx->refDist[0] = (curFrame->poc - refFrames[1]->poc + 512) & 511;
    ctx->refDist[2] = (curFrame->poc - refFrames[1]->poc + 513) & 511;
    ctx->refDist[1] = (refFrames[0]->poc - curFrame->poc + 511) & 511;
    ctx->refDist[3] = (refFrames[0]->poc - curFrame->poc + 512) & 511;    
    if( ctx->refDist[0] == 0 )      // 实为码流错误, 为增强容错性, 继续解码
        ctx->refDist[0] = 2;
    if( ctx->refDist[2] == 0 )
        ctx->refDist[2] = 3;
    if( ctx->refDist[1] == 0 )
        ctx->refDist[1] = 1; 
    if( ctx->refDist[3] == 0 )
        ctx->refDist[3] = 2;
    ctx->denDist[-1] = 512;
    ctx->denDist[0] = 512 / ctx->refDist[0];
    ctx->denDist[1] = 512 / ctx->refDist[1];
    ctx->denDist[2] = 512 / ctx->refDist[2];
    ctx->denDist[3] = 512 / ctx->refDist[3];
    ctx->backMvScale[0] = ctx->refDist[3] * ctx->denDist[0];
    ctx->backMvScale[1] = 0;
    ctx->backMvScale[2] = ctx->refDist[1] * ctx->denDist[2];
    ctx->backMvScale[3] = 0;

    // 第二场所在位置
    if( ctx->picHdr.top_field_first )
    {
        ADD3X( ctx->picPlane, curFrame->plane, framePitchs );
    }
    else
    {
        COPY3X( ctx->picPlane, curFrame->plane );
    }

    // slice 逐一解码
    for( ; sIdx < sliVec.size(); sIdx++ )
    {
        (*pfn_DecSlice)( ctx, sliVec[sIdx].data, sliVec[sIdx].size );
    }
}

// 配置异步解码任务
void FrmDecTask::config( PFN_DecFrame pfnDec, FrmDecContext* ctx )
{
    m_pfnDecFrame = pfnDec;
    m_frameCtx = ctx;
}

// 异步解码任务
void FrmDecTask::work()
{
    assert( m_pfnDecFrame != nullptr && m_frameCtx != nullptr );
    (*m_pfnDecFrame)( m_frameCtx );
    m_frameCtx->curFrame->decState->set_frame_done();
}

//======================================================================================================================
extern void dec_macroblock_PSkip( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P16x16( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P16x8( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P8x16( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P8x8( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P16x16_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P16x8_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P8x16_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_P8x8_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_BSkip( FrmDecContext*, int mx, int my );
extern void dec_macroblock_BDirect( FrmDecContext* ctx, int mx, int my );
extern void dec_macroblock_B16x16( FrmDecContext*, int mx, int my );
extern void dec_macroblock_B16x8( FrmDecContext*, int mx, int my );
extern void dec_macroblock_B8x16( FrmDecContext*, int mx, int my );
extern void dec_macroblock_B8x8( FrmDecContext*, int mx, int my );
extern void dec_macroblock_BSkip_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_BDirect_AEC( FrmDecContext* ctx, int mx, int my );
extern void dec_macroblock_B16x16_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_B16x8_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_B8x16_AEC( FrmDecContext*, int mx, int my );
extern void dec_macroblock_B8x8_AEC( FrmDecContext*, int mx, int my );

// 缺省解码回调函数
static void default_codec_notify( int, void*, void* )
{}

AvsContext::AvsContext( const IrkAvsDecConfig* cfg, int sseVer ) : workingQueue(MAX_THEAD_CNT)
{
    this->config = *cfg;
    this->sseVersion = sseVer;
    this->pfnNotify = &default_codec_notify;
    this->notifyParam = nullptr;

    // 帧内预测函数
    this->pfnLumaIPred[0] = &intra_pred_ver;
    this->pfnLumaIPred[1] = &intra_pred_hor;
    this->pfnLumaIPred[2] = &intra_pred_dc;
    this->pfnLumaIPred[3] = &intra_pred_downleft;
    this->pfnLumaIPred[4] = &intra_pred_downright;
    this->pfnCbCrIPred[0] = &intra_pred_dc;
    this->pfnCbCrIPred[1] = &intra_pred_hor;
    this->pfnCbCrIPred[2] = &intra_pred_ver;
    this->pfnCbCrIPred[3] = &intra_pred_plane;

    // P 宏块解码函数
    this->pfnDecMbP[0] = &dec_macroblock_PSkip;
    this->pfnDecMbP[1] = &dec_macroblock_P16x16;
    this->pfnDecMbP[2] = &dec_macroblock_P16x8;
    this->pfnDecMbP[3] = &dec_macroblock_P8x16;
    this->pfnDecMbP[4] = &dec_macroblock_P8x8;
    this->pfnDecMbP_AEC[0] = &dec_macroblock_PSkip;
    this->pfnDecMbP_AEC[1] = &dec_macroblock_P16x16_AEC;
    this->pfnDecMbP_AEC[2] = &dec_macroblock_P16x8_AEC;
    this->pfnDecMbP_AEC[3] = &dec_macroblock_P8x16_AEC;
    this->pfnDecMbP_AEC[4] = &dec_macroblock_P8x8_AEC;

    // B 宏块解码函数
    this->pfnDecMbB[0] = &dec_macroblock_BSkip;
    this->pfnDecMbB[1] = &dec_macroblock_BDirect;
    this->pfnDecMbB[2] =
    this->pfnDecMbB[3] =
    this->pfnDecMbB[4] = &dec_macroblock_B16x16;
    this->pfnDecMbB[5] =
    this->pfnDecMbB[7] =
    this->pfnDecMbB[9] =
    this->pfnDecMbB[11] =
    this->pfnDecMbB[13] =
    this->pfnDecMbB[15] =
    this->pfnDecMbB[17] =
    this->pfnDecMbB[19] =
    this->pfnDecMbB[21] = &dec_macroblock_B16x8;
    this->pfnDecMbB[6] =
    this->pfnDecMbB[8] =
    this->pfnDecMbB[10] =
    this->pfnDecMbB[12] =
    this->pfnDecMbB[14] =
    this->pfnDecMbB[16] =
    this->pfnDecMbB[18] =
    this->pfnDecMbB[20] =
    this->pfnDecMbB[22] = &dec_macroblock_B8x16;
    this->pfnDecMbB[23] = &dec_macroblock_B8x8;
    this->pfnDecMbB_AEC[0] = &dec_macroblock_BSkip_AEC;
    this->pfnDecMbB_AEC[1] = &dec_macroblock_BDirect_AEC;
    this->pfnDecMbB_AEC[2] =
    this->pfnDecMbB_AEC[3] =
    this->pfnDecMbB_AEC[4] = &dec_macroblock_B16x16_AEC;
    this->pfnDecMbB_AEC[5] =
    this->pfnDecMbB_AEC[7] =
    this->pfnDecMbB_AEC[9] =
    this->pfnDecMbB_AEC[11] =
    this->pfnDecMbB_AEC[13] =
    this->pfnDecMbB_AEC[15] =
    this->pfnDecMbB_AEC[17] =
    this->pfnDecMbB_AEC[19] =
    this->pfnDecMbB_AEC[21] = &dec_macroblock_B16x8_AEC;
    this->pfnDecMbB_AEC[6] =
    this->pfnDecMbB_AEC[8] =
    this->pfnDecMbB_AEC[10] =
    this->pfnDecMbB_AEC[12] =
    this->pfnDecMbB_AEC[14] =
    this->pfnDecMbB_AEC[16] =
    this->pfnDecMbB_AEC[18] =
    this->pfnDecMbB_AEC[20] =
    this->pfnDecMbB_AEC[22] = &dec_macroblock_B8x16_AEC;
    this->pfnDecMbB_AEC[23] = &dec_macroblock_B8x8_AEC;

    this->threadCnt = 1;
    this->status = 0;
    this->frameWidth = 0;
    this->frameHeight = 0;
    this->skipNonRef = 0;
    this->refFrames[0] = nullptr;
    this->refFrames[1] = nullptr;
    this->outFrame = nullptr;
}

AvsContext::~AvsContext()
{
    // 丢弃已有的解码数据
    this->clear();

    // 关闭线程池
    if( this->threadCnt > 1 )
    {
        this->threadPool.shutdown();
    }
}

// 解码器初始化
bool AvsContext::setup()
{
    int threadCnt = this->config.thread_cnt;
    if( threadCnt == 1 )    // 单线程解码
    {
        this->threadCnt = 1;
        return true;
    }

    const int coreCnt = irk::cpu_core_count();  // CPU 核心数

    if( threadCnt <= 0 || threadCnt > coreCnt )
        threadCnt = coreCnt;
    if( threadCnt > MAX_THEAD_CNT )
        threadCnt = MAX_THEAD_CNT;

    if( this->threadPool.setup( threadCnt ) )   // 启动线程池
    {
        this->threadCnt = threadCnt;
        return true;
    }

    return false;
}

// 丢弃已有的解码数据
void AvsContext::clear()
{
    // 丢弃正在进行异步解码的帧
    if( this->workingQueue.count() > 0 )
    {
        FrmDecContext* ctx = nullptr;
        while( this->workingQueue.pop_front( &ctx ) )
        {
            ctx->curFrame->decState->wait_frame_done(); // 等待解码完成
            discard_frame_ctx( ctx );
        }
    }

    // 丢弃参考帧
    DISMISS_FRAME( this->refFrames[0] );
    DISMISS_FRAME( this->refFrames[1] );
    DISMISS_FRAME( this->outFrame );
}

}   // namespace irk_avs_dec

//======================================================================================================================
#ifdef __cplusplus
extern "C" {
#endif

using namespace irk_avs_dec;

// decode one AVS+ picture
// if succeeded return data size cosumed, 
// if failed return negtive error code,
// NOTE 1: decoded picture will be send to user by the notify callback,
//          notify callback will always be called in the thread calling this function
// NOTE 2: for multi-thread decoding, input NULL will flush cached pictures
IRK_AVSDEC_EXPORT int irk_avs_decoder_decode( IrkAvsDecoder* decoder, const IrkCodedPic* encPic )
{
    AvsContext* ctx = static_cast<AvsContext*>( decoder );

    if( !encPic )   // 用户要求输出缓存的帧
    {
        // 多线程下, 等待之前的所有帧完成解码
        if( !ctx->workingQueue.is_empty() )
        {
            wait_all_frames( ctx );
        }

        if( ctx->outFrame )
        {
            (*ctx->pfnNotify)( IRK_CODEC_DONE, ctx->outFrame, ctx->notifyParam );
            ctx->outFrame->dismiss();
            ctx->outFrame = nullptr;
        }
        return 0;
    }

    // 并行解码如果达到最大线程数, 等待最先的一帧帧完成解码
    if( ctx->workingQueue.count() >= ctx->threadCnt )
    {
        wait_one_frame( ctx );
    }
    assert( ctx->workingQueue.count() < ctx->threadCnt );

    // 创建当前帧解码 context
    FrmDecContext* frmCtx = create_frame_ctx( ctx );
    frmCtx->userPts = encPic->userpts;
    frmCtx->userData = encPic->userdata;
    FrmCtxGuard ctxGuard( frmCtx );         // assure resource-safe

    // sliDataVec 存储分割的 slice 数据
    frmCtx->sliceVec.clear();
    frmCtx->sliceVec.reserve( 16 );

    // 先复制到内部缓存, 预留 4K 空间, 避免错误的码流导致内存越界
    frmCtx->dataBuf.clear();
    if( frmCtx->dataBuf.capacity() < encPic->size + 4096 )
        frmCtx->dataBuf.reserve( encPic->size + 8192 );
    frmCtx->dataBuf.assign( encPic->data, encPic->size );

    uint8_t* picData = frmCtx->dataBuf.data();
    const int picSize = (int)encPic->size;
    *(uint64_t*)(picData + picSize) = 0;    // 填充 0 有助于解码时数据末尾检测

    // 查找第一个 start code
    int used = find_start_code( picData, picSize );

    // 逐一处理 start code unit
    while( used < picSize - 4 )
    {
        assert( (*(uint32_t*)(picData + used) & 0xFFFFFF) == 0x010000 );
        uint8_t* data = picData + used;
        const uint8_t scode = data[3];

        if( scode == 0xB0 )         // sequence header
        {
            if( frmCtx->curFrame )  // 当前已有一帧在解码, 这是下一帧的数据
                break;

            int size = find_next_start_code( data, picSize - used );
            used += size;
            int errc = begin_sequence_decoding( ctx, data, size );
            if( errc != 0 )
                return errc;      
        }
        else if( scode == 0xB3 )    // I picture header
        {
            if( frmCtx->curFrame )  // 当前已有一帧在解码, 这是下一帧的数据
                break;

            if( ctx->status < AVS_SEQ_HDR_PARSED )  // sequence header 未解析, 继续查找
            {
                used += find_next_start_code( data, picSize - used );
                continue;
            }
            
            // 解析 picture header
            int size = 0;
            used += split_and_purify_sc_unit( data, picSize - used, &size );
            if( !parse_pic_header_I( &frmCtx->picHdr, &ctx->seqHdr, data, size ) )
                return IRK_AVS_DEC_BAD_STREAM;

            // 开始新一帧的解码
            int errc = begin_frame_decoding( frmCtx );
            if( errc != 0 )
                return errc;
        }
        else if( scode == 0xB6 )    // PB picture header
        {
            if( frmCtx->curFrame )  // 当前已有一帧在解码, 这是下一帧的数据
                break;

            if( ctx->status < AVS_SEQ_HDR_PARSED )  // sequence header 未解析, 继续查找
            {
                used += find_next_start_code( data, picSize - used );
                continue;
            }

            // 解析 picture header
            int size = 0;
            used += split_and_purify_sc_unit( data, picSize - used, &size );
            if( !parse_pic_header_PB( &frmCtx->picHdr, &ctx->seqHdr, data, size ) )
                return IRK_AVS_DEC_BAD_STREAM;

            // 查看是否跳过 B 帧
            if( ctx->skipNonRef && frmCtx->picHdr.pic_type == PIC_TYPE_B )
            {
                used += find_next_picture( data, picSize - used );
                return used;
            }

            // 开始新一帧的解码
            int errc = begin_frame_decoding( frmCtx );
            if( errc != 0 )
                return errc;
        }
        else if( scode >= 0 && scode <= 0xAF )    // slice
        {
            if( !frmCtx->curFrame )     // picture header 未解析
            {
                used += find_next_start_code( data, picSize - used );
                continue;
            }

            // 分割出 slice 数据
            int size = 0;
            used += split_and_purify_sc_unit( data, picSize - used, &size );
            SliceData slice = { data, size };
            frmCtx->sliceVec.push_back( slice );
        }
        else
        {
            // 不处理, 直接跳过
            used += find_next_start_code( data, picSize - used );
        }
    }

    if( frmCtx->curFrame && frmCtx->sliceVec.size() > 0 )
    {
        const int picType = frmCtx->picHdr.pic_type;
        DecFrame* curFrame = frmCtx->curFrame;

        // 设置当前帧的参考帧列表
        if( picType != PIC_TYPE_I )
        {
            prepare_ref_frames( frmCtx );
        }

        // 如果当前是参考帧, 添加到全局参考帧列表
        if( picType != PIC_TYPE_B )
        {
            // 添加到参考帧列表
            DISMISS_FRAME( ctx->refFrames[1] );
            ctx->refFrames[1] = ctx->refFrames[0];
            ctx->refFrames[0] = curFrame;
            curFrame->add_ref();
        }

        if( ctx->threadCnt > 1 )    // 多线程异步解码
        {
            // 配置异步解码任务
            if( frmCtx->decTask == nullptr )
            {
                frmCtx->decTask = new FrmDecTask;
                frmCtx->decTask->add_ref();
            }
            if( picType == PIC_TYPE_I )
            {
                frmCtx->decTask->config( &decode_frame_I, frmCtx );
            }
            else if( picType == PIC_TYPE_P )
            {
                frmCtx->decTask->config( &decode_frame_P, frmCtx );
            }
            else
            {
                frmCtx->decTask->config( &decode_frame_B, frmCtx );
            }

            // 启动异步解码
            ctx->threadPool.run_task( frmCtx->decTask );

            // 添加到工作队列
            ctx->workingQueue.push_back( frmCtx );
            ctxGuard.detach();
        }
        else    // 单线程解码
        {
            if( picType == PIC_TYPE_I )
            {
                decode_frame_I( frmCtx );
            }
            else if( picType == PIC_TYPE_P )
            {
                decode_frame_P( frmCtx );
            }
            else
            {
                decode_frame_B( frmCtx );
            }

            // 结束当前帧解码
            curFrame->decState->set_frame_done();
            end_frame_decoding( frmCtx );
        }
    }

    return used;
}

// SSE1 = 100
// SSE2 = 200
// SSE3 = 300, SSSE3 = 301
// SSE4.1 = 401, SSE4.2 = 402
extern int get_sse_version();

// create AVS+ decoder
// return NULL if failed(CPU does not support SSE, create internal threads failed)
IRK_AVSDEC_EXPORT IrkAvsDecoder* irk_create_avs_decoder( const IrkAvsDecConfig* pCfg )
{
    // CPU needs SSE-4 support
    int sseVer = get_sse_version();
    if( sseVer < 401 )
        return nullptr;

    AvsContext* ctx = new AvsContext( pCfg, sseVer );
    if( !ctx->setup() )
    {
        delete ctx;
        return nullptr;
    }
    return ctx;
}

// destroy AVS+ decoder
// all resource allocated by the decoder will be destoryed
IRK_AVSDEC_EXPORT void irk_destroy_avs_decoder( IrkAvsDecoder* decoder )
{
    AvsContext* ctx = static_cast<AvsContext*>( decoder );
    delete ctx;
}

// reset AVS+ decoder(before decoding new bitstream)
// if "resetAll" == false, global setting such as sequece header will not be reset
IRK_AVSDEC_EXPORT void irk_avs_decoder_reset( IrkAvsDecoder* decoder, bool resetAll )
{
    AvsContext* ctx = static_cast<AvsContext*>( decoder );
    ctx->clear();
    if( resetAll )
        ctx->status = 0;    // clear sequence header  
}

// set decoding notify callback
// when got IRK_CODEC_DONE code, notify data point to IrkAvsDecedPic struct
IRK_AVSDEC_EXPORT void irk_avs_decoder_set_notify( IrkAvsDecoder* decoder, PFN_CodecNotify callback, void* cbparam )
{
    AvsContext* ctx = static_cast<AvsContext*>( decoder );
    ctx->pfnNotify = callback;
    ctx->notifyParam = cbparam;
}

// set skip mode, if "skip_mode" != 0, skip non-reference pictures
IRK_AVSDEC_EXPORT void irk_avs_decoder_set_skip( IrkAvsDecoder* decoder, int skip_mode )
{
    AvsContext* ctx = static_cast<AvsContext*>( decoder );
    ctx->skipNonRef = skip_mode;
}

// get AVS+ stream basic infomation
// if succeeded return 0, if failed return negtive error code
IRK_AVSDEC_EXPORT int irk_avs_decoder_get_info( IrkAvsDecoder* decoder, IrkAvsStreamInfo* pinfo )
{
    static const int s_FrameRates[2][16] = 
    { 
        { 1, 24000, 24, 25, 30000, 30, 50, 60000, 60, 1, 1, 1, 1, 1, 1, 1 },
        { 1, 1001,  1,  1,  1001,  1,  1,  1001,  1,  1, 1, 1, 1, 1, 1, 1 },
    };

    AvsContext* ctx = static_cast<AvsContext*>( decoder );
    if( ctx->status >= AVS_SEQ_HDR_PARSED )    // sequence header parsed
    {
        const AvsSeqHdr& seqHdr = ctx->seqHdr;
        pinfo->profile = seqHdr.profile;
        pinfo->level = seqHdr.level;
        pinfo->width = seqHdr.width;
        pinfo->height = seqHdr.height;
        pinfo->chroma_format = seqHdr.chroma_format;
        pinfo->frame_rate_num = s_FrameRates[0][seqHdr.frame_rate_code];
        pinfo->frame_rate_den = s_FrameRates[1][seqHdr.frame_rate_code];
        pinfo->bitrate = seqHdr.bitrate;
        pinfo->progressive_seq = seqHdr.progressive_seq;
        return 0;
    }
    return IRK_AVS_DEC_UNAVAILABLE;
}

// normally decoded picture is only valid in notify callback function,
// in case user wants to use decoded picture outside callback function, 
// user can either use custom memory allocator or retain the decoded picture
IRK_AVSDEC_EXPORT void irk_avs_decoder_retain_picture( IrkAvsDecedPic* pic )
{
    DecFrame* pframe = static_cast<DecFrame*>( pic );
    pframe->add_ref();
}

// retained picture must be released before decoder destroyed
IRK_AVSDEC_EXPORT void irk_avs_decoder_dismiss_picture( IrkAvsDecedPic* pic )
{
    DecFrame* pframe = static_cast<DecFrame*>( pic );
    pframe->dismiss();
}

#ifdef __cplusplus
}
#endif
