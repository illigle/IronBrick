#include "stdafx.h"
#include "UYVYReader.h"
#include <io.h>
#include <emmintrin.h>

UYVYReader::UYVYReader()
{
    m_FrameWidth = 0;
    m_FrameHeight = 0;
    m_FrameSize = 0;
    m_FrameCnt = 0;
    m_pLine = NULL;
    m_pFile = NULL;
}

UYVYReader::~UYVYReader()
{
    if( m_pFile )
        fclose( m_pFile );
    if( m_pLine )
        _aligned_free( m_pLine );
}

bool UYVYReader::OpenFile( const char* filename, int width, int height )
{
    if( m_pFile )
        Close();

    if( width & 1 )
        return false;

    FILE* fp = fopen( filename, "rb" );
    if( !fp )
        return false;

    int64_t filesize = _filelengthi64( _fileno( fp ) );
    if( filesize <= 0 )
    {
        fclose( fp );
        return false;
    }

    m_FrameWidth = width;
    m_FrameHeight = height;
    m_FrameSize = width * height * 2;
    m_FrameCnt = (int)(filesize / m_FrameSize);
    m_pLine = (uint8_t*)_aligned_malloc( width * 2 + 16, 16 );
    m_pFile = fp;

    return true;
}

void UYVYReader::Close()
{
    if( m_pFile )
    {
        m_FrameWidth = 0;
        m_FrameHeight = 0;
        m_FrameCnt = 0;
        m_FrameSize = 0;
        fclose( m_pFile );
        m_pFile = NULL;
    }

    if( m_pLine )
    {
        _aligned_free( m_pLine );
        m_pLine = NULL;
    }
}

bool UYVYReader::GetFrameRaw( int idx, uint8_t* pDst )
{
    if( m_pFile && idx >= 0 && idx < m_FrameCnt )
    {
        int64_t offset = (int64_t)idx * m_FrameSize;
        _fseeki64( m_pFile, offset, SEEK_SET );
        if( fread( pDst, 1, m_FrameSize, m_pFile ) == m_FrameSize )
            return true;
    }
    return false;
}

bool UYVYReader::GetFrameRawLuma( int idx, uint8_t* pDst )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );

    const int width2 = m_FrameWidth * 2;
    for( int i = 0; i < m_FrameHeight; i++ )
    {
        fread( m_pLine, 1, width2, m_pFile );
        for( int j = 1; j < width2; j += 4 )
        {
            *pDst++ = m_pLine[j];
            *pDst++ = m_pLine[j+2];
        }
    }

    return true;
}

bool UYVYReader::GetFrameYUVP420( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    uint8_t* yplane = pFrame->planes[0];
    uint8_t* uplane = pFrame->planes[1];
    uint8_t* vplane = pFrame->planes[2];
    const int ypitch = pFrame->pitch[0];
    const int uvWidth = m_FrameWidth >> 1;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );
    for( int i = 0; i < m_FrameHeight; i += 2 )
    {
        fread( m_pLine, 1, m_FrameWidth * 2, m_pFile );
        for( int j = 0; j < uvWidth; j++ )
        {
            uplane[j] = m_pLine[4*j];
            vplane[j] = m_pLine[4*j+2];
            yplane[2*j] = m_pLine[4*j+1];
            yplane[2*j+1] = m_pLine[4*j+3];
        }

        yplane += ypitch;
        fread( m_pLine, 1, m_FrameWidth * 2, m_pFile );
        for( int j = 0; j < m_FrameWidth; j++ )
            yplane[j] = m_pLine[2*j+1];

        yplane += ypitch;
        uplane += pFrame->pitch[1];
        vplane += pFrame->pitch[2];
    }

    pFrame->width[0] = m_FrameWidth;
    pFrame->width[1] = uvWidth;
    pFrame->width[2] = uvWidth;
    pFrame->height[0] = m_FrameHeight;
    pFrame->height[1] = m_FrameHeight >> 1;
    pFrame->height[2] = m_FrameHeight >> 1;
    return true;
}

bool UYVYReader::GetFrameYUVP422( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    uint8_t* yplane = pFrame->planes[0];
    uint8_t* uplane = pFrame->planes[1];
    uint8_t* vplane = pFrame->planes[2];
    const int uvWidth = m_FrameWidth >> 1;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );
    for( int i = 0; i < m_FrameHeight; i++ )
    {
        fread( m_pLine, 1, m_FrameWidth * 2, m_pFile );
        for( int j = 0; j < uvWidth; j++ )
        {
            uplane[j] = m_pLine[4*j];
            vplane[j] = m_pLine[4*j+2];
            yplane[2*j] = m_pLine[4*j+1];
            yplane[2*j+1] = m_pLine[4*j+3];
        }

        yplane += pFrame->pitch[0];
        uplane += pFrame->pitch[1];
        vplane += pFrame->pitch[2];
    }

    pFrame->width[0] = m_FrameWidth;
    pFrame->width[1] = uvWidth;
    pFrame->width[2] = uvWidth;
    pFrame->height[0] = m_FrameHeight;
    pFrame->height[1] = m_FrameHeight;
    pFrame->height[2] = m_FrameHeight;
    return true;
}

bool UYVYReader::GetFrameYUVP444( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    uint8_t* yplane = pFrame->planes[0];
    uint8_t* uplane = pFrame->planes[1];
    uint8_t* vplane = pFrame->planes[2];
    const int width2 = m_FrameWidth * 2;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );
    for( int i = 0; i < m_FrameHeight; i++ )
    {
        fread( m_pLine, 1, width2, m_pFile );
        *(int*)(m_pLine + width2) = *(int*)(m_pLine + width2 - 4);

        for( int j = 0; j < m_FrameWidth; j += 2 )
        {
            int k = j + j;
            uplane[j] = m_pLine[k];
            uplane[j+1] = (m_pLine[k] + m_pLine[k+4] + 1) >> 1;
            vplane[j] = m_pLine[k+2];
            vplane[j+1] = (m_pLine[k+2] + m_pLine[k+6] + 1) >> 1;
            yplane[j] = m_pLine[k+1];
            yplane[j+1] = m_pLine[k+3];
        }

        yplane += pFrame->pitch[0];
        uplane += pFrame->pitch[1];
        vplane += pFrame->pitch[2];
    }

    pFrame->width[0] = m_FrameWidth;
    pFrame->width[1] = m_FrameWidth;
    pFrame->width[2] = m_FrameWidth;
    pFrame->height[0] = m_FrameHeight;
    pFrame->height[1] = m_FrameHeight;
    pFrame->height[2] = m_FrameHeight;
    return true;
}

bool UYVYReader::GetFrameBGRA( int idx, uint8_t* pGBRA, int dstPitch )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );

    // set coeff for HD or SD
    const int width2 = m_FrameWidth * 2;
    int ku = 2165, kum = -218, kv = 1836, kvm = -547;
    if( m_FrameHeight < 720 )       // ITU-R 601
    {
        ku = 2065;
        kum = -401;
        kv = 1634;
        kvm = -832;
    }

    ALIGNAS(16) int bgra[4];
    bgra[3] = 0;
    
    // UYVY 不是一种常见的格式, 故没有进行过多优化
    for( int i = 0; i < m_FrameHeight; i++ )
    {
        int32_t* dst = (int32_t*)(pGBRA + dstPitch * i);

        fread( m_pLine, 1, width2, m_pFile );
        *(int*)(m_pLine + width2) = *(int*)(m_pLine + width2 - 4);

        for( int j = 0; j < m_FrameWidth; j += 2 )
        {
            int k = j + j;
            int y = 1192 * (m_pLine[k+1] - 16) + 512; // 512 for round
            int u = m_pLine[k] - 128;
            int v = m_pLine[k+2] - 128;
            bgra[0] = y + ku * u;
            bgra[1] = y + kvm * v + kum * u;
            bgra[2] = y + kv * v;

            __m128i val = _mm_load_si128( (__m128i*)bgra );
            val = _mm_srai_epi32( val, 10 );
            val = _mm_packs_epi32( val, val );
            val = _mm_packus_epi16( val, val );
            dst[j] = _mm_cvtsi128_si32( val );

            y = 1192 * (m_pLine[k+3] - 16) + 512;
            u = ((m_pLine[k] + m_pLine[k+4] + 1) >> 1) - 128;
            v = ((m_pLine[k+2] + m_pLine[k+6] + 1) >> 1) - 128;
            bgra[0] = y + ku * u;
            bgra[1] = y + kvm * v + kum * u;
            bgra[2] = y + kv * v;

            val = _mm_load_si128( (__m128i*)bgra );
            val = _mm_srai_epi32( val, 10 );
            val = _mm_packs_epi32( val, val );
            val = _mm_packus_epi16( val, val );
            dst[j+1] = _mm_cvtsi128_si32( val );
        }
    }

    return true;
}