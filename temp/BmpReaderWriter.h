#ifndef _BMPREADERWRITER_H_
#define _BMPREADERWRITER_H_

#include "CommonDefine.h"

//========== BMP 图片行宽总是 4 字节的倍数 =============
class SmtBmpReaderWriter
{
public:
    SmtBmpReaderWriter();
    ~SmtBmpReaderWriter();

    // read bmp file into internal buffer; reset clip rect
    bool ReadBmp( const char* szFileName );

    // write image stored in internal buffer to bmp file; only clipped rect will be writen out
    bool WriteBmp( const char* szFileName );

    // store rgb image to internal buffer, <pitch>: source image's line width in byte; reset clip rect
    void StoreImage( const uint8_t* pImg, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bits );

    // create an image, its contents is undefined; current image will be destroyed, reset clip rect
    void CreateImage( uint32_t width, uint32_t height, uint32_t bits );

    // clip image to specified rect
    void ClipImage( uint32_t left, uint32_t top, uint32_t right, uint32_t bottom );

    // convert stored image to 8 bits bmp, whole image will be converted
    bool Convert8();

    // convert stored image to 24 bits bmp, whole image will be converted
    bool Convert24();

    // convert stored image to 32 bits bmp, whole image will be converted
    bool Convert32();

    uint8_t*    GetData()                       { return m_Data;    }
    uint32_t    GetWidth() const                { return m_Width;   }       // in pixel
    uint32_t    GetHeight() const               { return m_Height;  }       // in pixel
    uint32_t    GetPitch() const                { return m_Pitch;   }       // in byte
    uint32_t    GetBits() const                 { return m_Bits;    }
    void        GetClipRect( uint32_t*, uint32_t*, uint32_t*, uint32_t* ) const;

private:
    uint8_t*    m_Data;
    uint32_t    m_BuffSize;
    uint32_t    m_ClipX;
    uint32_t    m_ClipY;
    uint32_t    m_ClipWidth;
    uint32_t    m_ClipHeight;
    uint32_t    m_Width;
    uint32_t    m_Pitch;
    uint32_t    m_Height;
    uint32_t    m_Bits;
};

inline SmtBmpReaderWriter::SmtBmpReaderWriter() 
: m_Data(0), m_BuffSize(0), m_ClipX(0), m_ClipY(0), m_ClipWidth(0), m_ClipHeight(0)
, m_Width(0), m_Pitch(0), m_Height(0), m_Bits(0) 
{}

inline SmtBmpReaderWriter::~SmtBmpReaderWriter()  
{ 
    delete[] m_Data; 
}

inline void SmtBmpReaderWriter::GetClipRect( uint32_t* left, uint32_t* top, uint32_t* right, uint32_t* bottom ) const
{
    *left = m_ClipX;
    *right = m_ClipX + m_ClipWidth;
    *top = m_ClipY;
    *bottom = m_ClipY + m_ClipHeight;
}

#endif