#ifndef _DX11DEVICE_H_
#define _DX11DEVICE_H_

#include "YUVReader.h"

// using DX11 or DX9
#define USING_DX11_DEVICE 1

struct DxRect
{
    int left;
    int top;
    int right;
    int bottom;
};

//=====================================================================================================================================
#ifdef USING_DX11_DEVICE

struct DX11Context;

class DX11Device
{
public:
    DX11Device();
    ~DX11Device();

    bool InitDevice( void* hwnd, int width, int height );
    void CloseDevice();

    bool DrawBGRA( const uint8_t* pData, int width, int height, int pitch, DxRect* pRect = NULL );
    bool DrawLuma( const uint8_t* pData, int width, int height, int pitch, DxRect* pRect = NULL );
    bool DrawYUVP( const YUVFrame* pFrame, DxRect* pRect = NULL );

    void Present();

private:
    DX11Context*    m_pDxCtx;
    void*           m_hwnd;
    bool            m_DevInited;
    int             m_WinWidth;
    int             m_WinHeight;
    int             m_FrameWidth;
    int             m_FrameHeight;
    int             m_LumaWidth;
    int             m_LumaHeight;
    YUVFrame        m_LastFrame;
};

#else

struct DX9Context;

class DX9Device
{
public:
    DX9Device();
    ~DX9Device();

    bool InitDevice( void* hwnd, int width, int height );
    void CloseDevice();

    bool DrawBGRA( const uint8_t* pData, int width, int height, int pitch, DxRect* pRect = NULL );
    bool DrawLuma( const uint8_t* pData, int width, int height, int pitch, DxRect* pRect = NULL );
    bool DrawYUVP( const YUVFrame* pFrame, DxRect* pRect = NULL );

    void Present();

private:
    DX9Context* m_pDxCtx;
    void*       m_hwnd;
    bool        m_DevInited;
    int         m_WinWidth;
    int         m_WinHeight;
    int         m_FrameWidth;
    int         m_FrameHeight;
    int         m_LumaWidth;
    int         m_LumaHeight;
    YUVFrame    m_LastFrame;
};

#endif

#endif