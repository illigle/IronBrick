#include "stdafx.h"
#include "Y4mParser.h"
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <io.h>
#include <intrin.h>

#ifdef _MSC_VER
#pragma intrinsic( memcmp, memcpy, memset )
#endif

Y4mParser::Y4mParser()
{
    m_FrameWidth = 0;
    m_FrameHeight = 0;
    m_FrameSize = 0;
    m_ChromaType = YUV_ChromaType_420;
    m_YSize = 0;
    m_FrameRateNum = 25;
    m_FrameRateDen = 1;
    m_FrameStruct = YUV_Progressive;
    m_FrameOffset = 0;
    m_FrameCnt = 0;
    m_pYPlane = NULL;
    m_pUPlane = NULL;
    m_pVPlane = NULL;
    m_pFile = NULL;
}

Y4mParser::~Y4mParser()
{
    if( m_pFile )
        fclose( m_pFile );

    if( m_pYPlane )
        _aligned_free( m_pYPlane );
    if( m_pUPlane )
        _aligned_free( m_pUPlane );
    if( m_pVPlane )
        _aligned_free( m_pVPlane );
}

bool Y4mParser::OpenFile( const char* filename )
{
    if( m_pFile )
        Close();

    FILE* fp = fopen( filename, "rb" );
    if( !fp )
        return false;

    char buff[256];
    if( fread( buff, 1, 256, fp ) != 256 )
        goto err_exit;

    // check file
    if( memcmp( buff, "YUV4MPEG2 ", 10 ) )
        goto err_exit;

    // parse field
    int pos = 10;
    do
    {
        int len = ParseField( buff + pos, 256 - pos );
        pos += len;
    }
    while( buff[pos-1] != 0xA && pos < 240 );  // 0xA : end of header

    // all field parsed ?
    if( pos >= 240 || memcmp( buff+pos, "FRAME", 5 ) || buff[pos+5] != 0xA )
        goto err_exit;

    // frame size
    m_YSize = m_FrameWidth * m_FrameHeight;
    if( m_ChromaType == YUV_ChromaType_420 )
        m_FrameSize = m_YSize * 3 / 2;
    else if( m_ChromaType == YUV_ChromaType_422 )
        m_FrameSize = m_YSize * 2;
    else
        m_FrameSize = m_YSize * 3;

    // calculate frame count
    int64_t filesize = _filelengthi64( _fileno( fp ) );
    if( filesize <= 0 )
        goto err_exit;

    m_FrameOffset = pos;
    m_FrameCnt = (int)( (filesize - pos) / (m_FrameSize + 6) );
   
    m_pFile = fp;
    return true;

err_exit:
    fclose( fp );
    return false;
}

void Y4mParser::Close()
{
    if( m_pFile )
    {
        m_FrameWidth = 0;
        m_FrameHeight = 0;
        m_ChromaType = YUV_ChromaType_420;
        m_FrameCnt = 0;
        m_FrameSize = 0;
        m_YSize = 0;
        m_FrameRateNum = 25;
        m_FrameRateDen = 1;
        m_FrameStruct = YUV_Progressive;
        m_FrameOffset = 0;
        fclose( m_pFile );
        m_pFile = NULL;
    }

    if( m_pYPlane )
    {
        _aligned_free( m_pYPlane );
        m_pYPlane = NULL;
    }
    if( m_pUPlane )
    {
        _aligned_free( m_pUPlane );
        m_pUPlane = NULL;
    }
    if( m_pVPlane )
    {
        _aligned_free( m_pVPlane );
        m_pVPlane = NULL;
    }
}

int Y4mParser::ParseField( const char* field, int size )
{
    int i = 0;
    for( ; i < size && field[i] != 0x20 && field[i] != 0xA; i++ )
    {}

    if( i < size )
    {
        int j = 0, val = 0;

        switch( field[0] )
        {
        case 'C':
            if( field[2] == '2' && field[3] == '2' )
                m_ChromaType = YUV_ChromaType_422;
            else if( field[2] == '4' && field[3] == '4' )
                m_ChromaType = YUV_ChromaType_444;
            else
                m_ChromaType = YUV_ChromaType_420;
            break;
        case 'F':
            for( j = 1; j < i && field[j] != ':'; j++ )
                val = val * 10 + field[j] - '0';
            m_FrameRateNum = val;
            val = 0;
            for( j += 1; j < i; j++ )
                val = val * 10 + field[j] - '0';
            m_FrameRateDen = val;
            break;
        case 'H':
            for( j = 1; j < i; j++ )
                val = val * 10 + field[j] - '0';
            m_FrameHeight = val;
            break;
        case 'I':  
            if( field[1] == 't' )
                m_FrameStruct = YUV_Top_Field_First;
            else if( field[1] == 'b' )
                m_FrameStruct = YUV_Bot_Field_First;
            else
                m_FrameStruct = YUV_Progressive;
            break;
        case 'W':
            for( j = 1; j < i; j++ )
                val = val * 10 + field[j] - '0';
            m_FrameWidth = val;
            break;
        }
    }

    return i + 1;
}

//============================================================================================================================
bool Y4mParser::GetFrameRaw( int idx, uint8_t* pDst )
{
    if( m_pFile && idx >= 0 && idx < m_FrameCnt )
    {
        char buff[8];
        int64_t offset = (int64_t)idx * (m_FrameSize + 6);
        _fseeki64( m_pFile, m_FrameOffset + offset, SEEK_SET );
        fread( buff, 1, 6, m_pFile );
        if( memcmp( buff, "FRAME", 5 ) )
            return false;

        if( fread( pDst, 1, m_FrameSize, m_pFile ) == m_FrameSize )
            return true;
    }
    return false;
}

bool Y4mParser::GetFrameRawLuma( int idx, uint8_t* pDst )
{
    if( m_pFile && idx >= 0 && idx < m_FrameCnt )
    {
        char buff[8];
        int64_t offset = (int64_t)idx * (m_FrameSize + 6);
        _fseeki64( m_pFile, m_FrameOffset + offset, SEEK_SET );
        fread( buff, 1, 6, m_pFile );
        if( memcmp( buff, "FRAME", 5 ) )
            return false;

        if( fread( pDst, 1, m_YSize, m_pFile ) == m_YSize )
            return true;
    }
    return false;
}

bool Y4mParser::GetFrameYUVP420( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    char buff[8];
    int64_t offset = (int64_t)idx * (m_FrameSize + 6);
    _fseeki64( m_pFile, m_FrameOffset + offset, SEEK_SET );
    fread( buff, 1, 6, m_pFile );
    if( memcmp( buff, "FRAME", 5 ) != 0 || buff[5] != 0xA )
        return false;

    return ReadYUV420( m_pFile, m_FrameWidth, m_FrameHeight, m_ChromaType, 8, pFrame );
}

bool Y4mParser::GetFrameYUVP422( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    char buff[8];
    int64_t offset = (int64_t)idx * (m_FrameSize + 6);
    _fseeki64( m_pFile, m_FrameOffset + offset, SEEK_SET );
    fread( buff, 1, 6, m_pFile );
    if( memcmp( buff, "FRAME", 5 ) != 0 || buff[5] != 0xA )
        return false;

    return ReadYUV422( m_pFile, m_FrameWidth, m_FrameHeight, m_ChromaType, 8, pFrame );
}

bool Y4mParser::GetFrameYUVP444( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    char buff[8];
    int64_t offset = (int64_t)idx * (m_FrameSize + 6);
    _fseeki64( m_pFile, m_FrameOffset + offset, SEEK_SET );
    fread( buff, 1, 6, m_pFile );
    if( memcmp( buff, "FRAME", 5 ) != 0 || buff[5] != 0xA )
        return false;

    return ReadYUV444( m_pFile, m_FrameWidth, m_FrameHeight, m_ChromaType, 8, pFrame );
}

bool Y4mParser::GetFrameBGRA( int idx, uint8_t* pDst, int dstPitch )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    // read raw frame
    int uvSize = m_YSize;
    if( m_ChromaType == YUV_ChromaType_420 )
        uvSize >>= 2;
    else if( m_ChromaType == YUV_ChromaType_422 )
        uvSize >>= 1;

    if( !m_pYPlane )
    {
        m_pYPlane = (uint8_t*)_aligned_malloc( m_YSize + 16, 16 );
        m_pUPlane = (uint8_t*)_aligned_malloc( uvSize + 16, 16 );
        m_pVPlane = (uint8_t*)_aligned_malloc( uvSize + 16, 16 );
    }

    int64_t offset = (int64_t)idx * (m_FrameSize + 6);
    _fseeki64( m_pFile, m_FrameOffset + offset, SEEK_SET );
    fread( m_pYPlane, 1, 6, m_pFile );
    if( memcmp( m_pYPlane, "FRAME", 5 ) != 0 || m_pYPlane[5] != 0xA )
        return false;
    if( fread( m_pYPlane, 1, m_YSize, m_pFile ) != m_YSize )
        return false;
    if( fread( m_pUPlane, 1, uvSize, m_pFile ) != uvSize )
        return false;
    if( fread( m_pVPlane, 1, uvSize, m_pFile ) != uvSize )
        return false;

    YUVFrame frame;
    frame.planes[0] = m_pYPlane;
    frame.planes[1] = m_pUPlane;
    frame.planes[2] = m_pVPlane;

    // convert YUV to BGR
    if( m_ChromaType == YUV_ChromaType_420 )
    {
        frame.width[0] = m_FrameWidth;
        frame.width[1] = m_FrameWidth >> 1;
        frame.width[2] = m_FrameWidth >> 1;
        frame.height[0] = m_FrameHeight;
        frame.height[1] = m_FrameHeight >> 1;
        frame.height[2] = m_FrameHeight >> 1;
        frame.pitch[0] = frame.width[0];
        frame.pitch[1] = frame.width[1];
        frame.pitch[2] = frame.width[2];

        FastConvertYUV420_BGRA( &frame, pDst, dstPitch );
    }
    else if( m_ChromaType == YUV_ChromaType_422 )
    {
        frame.width[0] = m_FrameWidth;
        frame.width[1] = m_FrameWidth >> 1;
        frame.width[2] = m_FrameWidth >> 1;
        frame.height[0] = m_FrameHeight;
        frame.height[1] = m_FrameHeight;
        frame.height[2] = m_FrameHeight;
        frame.pitch[0] = frame.width[0];
        frame.pitch[1] = frame.width[1];
        frame.pitch[2] = frame.width[2];

        FastConvertYUV422_BGRA( &frame, pDst, dstPitch );
    }
    else
    {
        frame.width[0] = m_FrameWidth;
        frame.width[1] = m_FrameWidth;
        frame.width[2] = m_FrameWidth;
        frame.height[0] = m_FrameHeight;
        frame.height[1] = m_FrameHeight;
        frame.height[2] = m_FrameHeight;
        frame.pitch[0] = frame.width[0];
        frame.pitch[1] = frame.width[1];
        frame.pitch[2] = frame.width[2];

        FastConvertYUV444_BGRA( &frame, pDst, dstPitch );
    }

    return true;
}
