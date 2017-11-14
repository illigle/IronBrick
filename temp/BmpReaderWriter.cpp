#include "stdafx.h"
#include "BmpReaderWriter.h"
#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma intrinsic( strcmp, strlen, memset, memcpy )
#endif

#define READ_UINT32(data, i)        *(uint32_t*)((uint8_t*)(data) + (i))
#define READ_UINT16(data, i)        *(uint16_t*)((uint8_t*)(data) + (i))
#define WRITE_UINT32(data, val)     *(uint32_t*)(data) = (val);
#define WRITE_UINT16(data, val)     *(uint16_t*)(data) = (uint16_t)(val);

bool SmtBmpReaderWriter::ReadBmp( const char* szFileName )
{
    FILE* fp = fopen( szFileName, "rb" );
    if( !fp )
    {
        dprintf( "open bmp file failed.\n" );
        return false;
    }

    uint8_t info[54];
    size_t cnt = fread( info, 1, 54, fp );
    if( cnt != 54 || info[0] != 0x42 || info[1] != 0x4D )
    {
        dprintf( "invalide bmp file." );
        fclose( fp );
        return false;
    }

    // read bmp file info
    uint32_t in_width = READ_UINT32( info, 18 );
    uint32_t in_height = READ_UINT32( info, 22 );
    uint32_t in_bits = READ_UINT16( info, 28 );
    
    if( in_bits != 8 && in_bits != 24 && in_bits != 32 )
    {
        dprintf( "invalide bmp file." );
        fclose( fp );
        return false;
    }
    if( in_bits == 8)
        fseek( fp, 1024, SEEK_CUR );    // skip palette 

    // check memory buffer's size
    uint32_t in_pitch = (in_width * in_bits / 8 + 3) & ~3;
    if( m_BuffSize < in_pitch * in_height )
    {
        delete[] m_Data;
        m_Data = new uint8_t[in_pitch * in_height + 3];     // + 3 是为了保证 clip 下输出也不会越界
        m_BuffSize = in_pitch * in_height;
    }

    // read img data
    uint8_t* dst_row = m_Data + (in_height - 1) * in_pitch;
    for( uint32_t i = 0; i < in_height; ++i )
    {
        fread( dst_row, 1, in_pitch, fp );
        dst_row -= in_pitch;                // bmp 图像是倒着放的
    }

    fclose( fp );

    m_Width = in_width;
    m_Pitch = in_pitch;
    m_Height = in_height;
    m_Bits = in_bits;
    m_ClipX = 0;
    m_ClipY = 0;
    m_ClipWidth = in_width;
    m_ClipHeight = in_height;

    return true;
}

bool SmtBmpReaderWriter::WriteBmp( const char* szFileName )
{
    if( !m_Data )
    {
        dprintf( "no image data.\n" );
        return false;
    }

    FILE* fp = fopen( szFileName, "wb" );
    if( !fp )
    {
        dprintf( "open bmp file failed.\n" );
        return false;
    }

    // create bmp header
    uint32_t lineByte = m_ClipWidth * (m_Bits / 8);
    uint32_t imgSize =  lineByte * m_ClipHeight;

    uint8_t info[54];
    memset( info, 0, sizeof(info) );
    info[0] = 0x42;
    info[1] = 0x4D;
    WRITE_UINT32( &info[2], imgSize + 54 );
    info[10] = 54;
    info[14] = 40;
    WRITE_UINT32( &info[18], m_ClipWidth );
    WRITE_UINT32( &info[22], m_ClipHeight );
    info[26] = 1;
    WRITE_UINT16( &info[28], (uint16_t)m_Bits );
    WRITE_UINT32( &info[34], imgSize );

    if( m_Bits == 8 )
    {
        WRITE_UINT32( &info[2], imgSize + 54 + 1024 );
        WRITE_UINT16( &info[10], 54 + 1024 );

        // create palette
        uint8_t bmi_table[1024];
        for( uint32_t i = 0; i < 1024; i += 4 )
        {
            uint8_t gray = (uint8_t)(i >> 2);
            bmi_table[i] = bmi_table[i+1] = bmi_table[i+2] = gray;
            bmi_table[i+3] = 0xFF;
        }
        
        fwrite( info, 1, 54, fp );
        fwrite( bmi_table, 1, 1024, fp );
    }
    else
    {
        fwrite( info, 1, 54, fp );
    }

    // write image data
    uint32_t out_pitch = (lineByte + 3) & ~3;               // bmp require 4 byte pitch
    DASSERT( out_pitch <= m_Pitch );

    uint8_t* src_row = m_Data + (m_ClipY + m_ClipHeight - 1) * m_Pitch + m_ClipX * m_Bits / 8;
    DASSERT( src_row + out_pitch <= m_Data + m_BuffSize + 3 );

    for( uint32_t i = 0; i < m_ClipHeight; ++i )
    {
        fwrite( src_row, 1, out_pitch, fp );
        src_row -= m_Pitch;
    }

    fclose( fp );

    return true;
}

void SmtBmpReaderWriter::StoreImage( const uint8_t* pImg, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bits )
{
    DASSERT( bits == 8 || bits == 24 || bits == 32 );

    uint32_t rowBytes = width * bits / 8;
    uint32_t dstPitch = (rowBytes + 3) & ~3;

    if( m_BuffSize < dstPitch * height )
    {
        delete[] m_Data;
        m_Data = new uint8_t[dstPitch * height + 3];
        m_BuffSize = dstPitch * height;
    }

    const uint8_t* pSrc = pImg;
    uint8_t* pDst = m_Data;
    for( uint32_t i = 0; i < height; ++i )
    {
        memcpy( pDst, pSrc, rowBytes );
        pSrc += pitch;
        pDst += dstPitch;
    }

    m_Width = width;
    m_Height = height;
    m_Pitch = dstPitch;
    m_Bits = bits;
    m_ClipX = 0;
    m_ClipY = 0;
    m_ClipWidth = width;
    m_ClipHeight = height;
}

void SmtBmpReaderWriter::CreateImage( uint32_t width, uint32_t height, uint32_t bits )
{
    uint32_t dstPitch = (width * bits / 8 + 3) & ~3;

    if( m_BuffSize < dstPitch * height )
    {
        delete[] m_Data;
        m_Data = new uint8_t[dstPitch * height + 3];
        m_BuffSize = dstPitch * height;
    }

    m_Width = width;
    m_Height = height;
    m_Pitch = dstPitch;
    m_Bits = bits;
    m_ClipX = 0;
    m_ClipY = 0;
    m_ClipWidth = width;
    m_ClipHeight = height;
}

void SmtBmpReaderWriter::ClipImage( uint32_t left, uint32_t top, uint32_t right, uint32_t bottom )
{
    DASSERT( left < right && right <= m_Width );
    DASSERT( top < bottom && bottom <= m_Height);

    m_ClipX = left;
    m_ClipY = top;
    m_ClipWidth = right - left;
    m_ClipHeight = bottom - top;
}

bool SmtBmpReaderWriter::Convert8()
{
    if( !m_Data )
        return false;

    uint32_t dstPitch = (m_Width + 3) & ~3;
    const uint8_t* pSrc = m_Data;
    uint8_t* pDst = m_Data;
    
    if( m_Bits == 24 )
    {
        for( uint32_t i = 0; i < m_Height; ++i )
        {
            for( uint32_t j = 0; j < m_Width; ++j )
            {
                pDst[j] = (uint8_t)((pSrc[3*j] + pSrc[3*j+1] + pSrc[3*j+2]) / 3);
            }
            pSrc += m_Pitch;
            pDst += dstPitch;
        }
    }
    else if( m_Bits == 32 )
    {
        for( uint32_t i = 0; i < m_Height; ++i )
        {
            for( uint32_t j = 0; j < m_Width; ++j )
            {
                pDst[j] = (uint8_t)((pSrc[4*j] + pSrc[4*j+1] + pSrc[4*j+2]) / 3);
            }
            pSrc += m_Pitch;
            pDst += dstPitch;
        }
    }

    m_Pitch = dstPitch;
    m_Bits = 8;
    return true;
}

bool SmtBmpReaderWriter::Convert24()
{
    if( !m_Data )
        return false;

    uint32_t dstPitch = (m_Width * 3 + 3) & ~3;

    if( m_Bits == 8 )
    {
        // new buffer needed
        uint8_t* pOldBuff = m_Data;
        m_Data = new uint8_t[dstPitch * m_Height + 3];
        m_BuffSize = dstPitch * m_Height;

        const uint8_t* pSrc = pOldBuff;
        uint8_t* pDst = m_Data;
        
        for( uint32_t i = 0; i < m_Height; ++i )
        {
            for( uint32_t j = 0; j < m_Width; ++j )
            {
                pDst[3*j] = pDst[3*j+1] = pDst[3*j+2] = pSrc[j];
            }
            pSrc += m_Pitch;
            pDst += dstPitch;
        }

        delete[] pOldBuff;
    }
    else if( m_Bits == 32 )
    {
        const uint8_t* pSrc = m_Data;
        uint8_t* pDst = m_Data;

        for( uint32_t i = 0; i < m_Height; ++i )
        {
            for( uint32_t j = 0; j < m_Width; ++j )
            {
                *(uint32_t*)(pDst + 3*j) = *(uint32_t*)(pSrc + 4*j);
            }
            pSrc += m_Pitch;
            pDst += dstPitch;
        }
    }

    m_Pitch = dstPitch;
    m_Bits = 24;
    return true;
}

bool SmtBmpReaderWriter::Convert32()
{
    if( !m_Data )
        return false;

    uint32_t dstPitch = m_Width * 4;

    if( m_Bits == 8 )
    {
        // new buffer needed
        uint8_t* pOldBuff = m_Data;
        m_Data = new uint8_t[dstPitch * m_Height + 3];
        m_BuffSize = dstPitch * m_Height;

        const uint8_t* pSrc = pOldBuff;
        uint8_t* pDst = m_Data;

        for( uint32_t i = 0; i < m_Height; ++i )
        {
            for( uint32_t j = 0; j < m_Width; ++j )
            {
                *(uint32_t*)(pDst + 4*j) = 0xFF000000 | pSrc[j] * 0x01010101u;
            }
            pSrc += m_Pitch;
            pDst += dstPitch;
        }

        delete[] pOldBuff;
    }
    else if( m_Bits == 24 )
    {
        // new buffer needed
        uint8_t* pOldBuff = m_Data;
        m_Data = new uint8_t[dstPitch * m_Height + 3];
        m_BuffSize = dstPitch * m_Height;

        const uint8_t* pSrc = pOldBuff;
        uint8_t* pDst = m_Data;

        for( uint32_t i = 0; i < m_Height; ++i )
        {
            for( uint32_t j = 0; j < m_Width; ++j )
            {
                *(uint32_t*)(pDst + 4*j) = 0xFF000000 | *(uint32_t*)(pSrc + 3*j);
            }
            pSrc += m_Pitch;
            pDst += dstPitch;
        }

        delete[] pOldBuff;
    }

    m_Pitch = dstPitch;
    m_Bits = 32;
    return false;
}