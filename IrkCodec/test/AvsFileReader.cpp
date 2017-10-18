#include "AvsFileReader.h"

#ifndef _MSC_VER
#define _fseeki64 fseeko
#endif

#define READ_SIZE (16*1024)

AvsFileReader::AvsFileReader()
{
    m_BufIdx = 0;
    m_pFile = nullptr;
}

AvsFileReader::~AvsFileReader()
{
    if( m_pFile )
    {
        ::fclose( m_pFile );
    }
}

bool AvsFileReader::open( const char* filename )
{
    m_DataBuf[0].clear();
    m_DataBuf[0].reserve( 1024 * 128 );
    m_DataBuf[1].clear();
    m_DataBuf[1].reserve( 1024 * 128 );
    m_BufIdx = 0;

    if( m_pFile )
    {
        ::fclose( m_pFile );
        m_pFile = nullptr;
    }
    m_pFile = ::fopen( filename, "rb" );
    return m_pFile != nullptr;
}

void AvsFileReader::close()
{
    if( m_pFile )
    {
        fclose( m_pFile );
        m_pFile = nullptr;
    }
}

int AvsFileReader::seek( int64_t offset )
{
    m_DataBuf[0].clear();
    m_DataBuf[1].clear();

    if( m_pFile )
    {
        ::rewind( m_pFile );
        return _fseeki64( m_pFile, offset, SEEK_SET );
    }
    return -1;
}

const uint8_t*  AvsFileReader::get_picture( size_t* psize )
{
    *psize = 0;
    if( !m_pFile )
        return nullptr;

    DataVec& dbuf = m_DataBuf[m_BufIdx];
    uint32_t startCode = 0;
    int i = 0;
    int k = -1;     // 图像起始位置
    while( 1 )
    {
        const uint8_t* data = dbuf.data();
        int size = (int)dbuf.size() - 3;

        // search for start code
        if( k < 0 )
        {
            for( ; i < size; i++ )
            {
                uint32_t sc = *(uint32_t*)(data + i);
                if( sc == 0xB0010000 || sc == 0xB3010000 || sc == 0xB6010000 )
                {
                    k = i;
                    i += 8;
                    startCode = sc;
                    break;
                }
            }
        }

        // search for next start code
        if( k >= 0 )
        {
            // search for picture header
            if( startCode != 0xB3010000 && startCode != 0xB6010000 )
            {
                for( ; i < size; i++ )
                {
                    uint32_t sc = *(uint32_t*)(data + i);
                    if( sc == 0xB3010000 || sc == 0xB6010000 )
                    {
                        i += 8;
                        startCode = sc;
                        break;
                    }
                }
            }

            // search for next picture
            if( startCode == 0xB3010000 || startCode == 0xB6010000 )
            {
                for( ; i < size; i++ )
                {
                    uint32_t sc = *(uint32_t*)(data + i);
                    if( sc == 0xB0010000 || sc == 0xB3010000 || sc == 0xB6010000 )
                    {
                        // 为下一帧做准备
                        m_BufIdx ^= 1;
                        m_DataBuf[m_BufIdx].assign( dbuf.data() + i, dbuf.size() - i );
                        *psize = i - k;
                        return data + k;
                    }
                }
            }
        }

        // 判断一下大小, 避免用户输入完全不相关的数据导致内存无限增长
        if( size > 8 * 1024 * 1024 )   
        {
            dbuf.clear();
            size = 0;
            i = 3;
            k = -1;
        }

        // read more data in
        uint8_t* pbuf = dbuf.alloc( READ_SIZE );
        size_t rdsize = ::fread( pbuf, 1, READ_SIZE, m_pFile );
        if( rdsize == 0 )
        {
            // 文件末尾的这一帧可能不是完整的一帧
            if( k >= 0 && size > k + 64 && (startCode==0xB3010000 || startCode==0xB6010000) )
            {
                m_BufIdx ^= 1;
                m_DataBuf[m_BufIdx].clear();
                *psize = dbuf.size() - k;
                return dbuf.data() + k;
            }
            break;
        }
        dbuf.commit( rdsize );
    }

    m_DataBuf[0].clear();
    m_DataBuf[1].clear();
    *psize = 0;
    return nullptr;
}
