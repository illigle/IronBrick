#include "stdafx.h"
#include "DXDevice.h"

#ifdef USING_DX11_DEVICE

#include <assert.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma intrinsic( memcpy, memset )
#pragma comment( lib, "d3d11" )
#pragma comment( lib, "D3DCompiler" )

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

struct DX11Context
{
    DX11Context()
    {
        memset( this, 0, sizeof( DX11Context ) );
    }

    ~DX11Context()
    {
        if( m_pLumaTexView )
            m_pLumaTexView->Release();
        if( m_pLumaTex )
            m_pLumaTex->Release();
        if( m_pYTexView )
            m_pYTexView->Release();
        if( m_pYTex )
            m_pYTex->Release();
        if( m_pUTexView )
            m_pUTexView->Release();
        if( m_pUTex )
            m_pUTex->Release();
        if( m_pVTexView )
            m_pVTexView->Release();
        if( m_pVTex )
            m_pVTex->Release();
        if( m_pFrameTexView )
            m_pFrameTexView->Release();
        if( m_pFrameTex )
            m_pFrameTex->Release();
        if( m_pDepthState )
            m_pDepthState->Release();
        if( m_pSampler )
            m_pSampler->Release();
        if( m_pVertexBuffer )
            m_pVertexBuffer->Release();
        if( m_pVertexLayout )
            m_pVertexLayout->Release();
        if( m_pBgrShader )
            m_pBgrShader->Release();
        if( m_pYuv709Shader )
            m_pYuv709Shader->Release();
        if( m_pYuv601Shader )
            m_pYuv601Shader->Release();
        if( m_pLumaShader )
            m_pLumaShader->Release();
        if( m_pVertexShader )
            m_pVertexShader->Release();
        if( m_pRTView )
            m_pRTView->Release();
        if( m_pImdCtx )
            m_pImdCtx->Release();
        if( m_pDevice )
            m_pDevice->Release();
    }

    ID3D11Device*                       m_pDevice;
    ID3D11DeviceContext*                m_pImdCtx;
    IDXGISwapChain*                     m_pSwapChain;
    ID3D11RenderTargetView*             m_pRTView;
    ID3D11VertexShader*                 m_pVertexShader;
    ID3D11PixelShader*                  m_pBgrShader;
    ID3D11PixelShader*                  m_pYuv709Shader;
    ID3D11PixelShader*                  m_pYuv601Shader;
    ID3D11PixelShader*                  m_pLumaShader;
    ID3D11InputLayout*                  m_pVertexLayout;
    ID3D11Buffer*                       m_pVertexBuffer;
    ID3D11SamplerState*                 m_pSampler;
    ID3D11DepthStencilState*            m_pDepthState;
    ID3D11Texture2D*                    m_pFrameTex;
    ID3D11ShaderResourceView*           m_pFrameTexView;
    ID3D11Texture2D*                    m_pYTex;
    ID3D11ShaderResourceView*           m_pYTexView;
    ID3D11Texture2D*                    m_pUTex;
    ID3D11ShaderResourceView*           m_pUTexView;
    ID3D11Texture2D*                    m_pVTex;
    ID3D11ShaderResourceView*           m_pVTexView;
    ID3D11Texture2D*                    m_pLumaTex;
    ID3D11ShaderResourceView*           m_pLumaTexView;
};

static float s_Vertex[] = 
{
    -1.f, -1.f, 0.f, 0.f, 1.f,
    -1.f, 1.f, 0.f, 0.f, 0.f,
    1.f, -1.f, 0.f, 1.f, 1.f,
    1.f, 1.f, 0.f, 1.f, 0.f,
};

//=================================================================================================================================
static HRESULT CompileShaderFromFile( const wchar_t* filename, const char* funcname, const char* shaderModel, ID3DBlob** ppOut )
{
    ID3DBlob* pErrorMsg = NULL;
    HRESULT hr = D3DCompileFromFile( filename, NULL, NULL, funcname, shaderModel, 
        D3DCOMPILE_ENABLE_STRICTNESS, 0, ppOut, &pErrorMsg );

    if( FAILED( hr ) )
    {
        if( pErrorMsg != NULL )
            printf( "Compile Shader Failed: %s\n", (char*)pErrorMsg->GetBufferPointer() );
    }
    if( pErrorMsg ) 
        pErrorMsg->Release();

    return hr;
}

DX11Device::DX11Device()
{
    m_pDxCtx = NULL;
    m_hwnd = NULL;
    m_WinWidth = 0;
    m_WinHeight = 0;
    m_FrameWidth = 0;
    m_FrameHeight = 0;
    m_LumaWidth = 0;
    m_LumaHeight = 0;
    memset( &m_LastFrame, 0, sizeof( m_LastFrame ) );
    m_DevInited = false;
}

DX11Device::~DX11Device()
{
    if( m_pDxCtx )
        delete m_pDxCtx;
}

bool DX11Device::InitDevice( void* hwnd, int width, int height )
{
    assert( hwnd != NULL && width > 0 && height > 0 );

    if( m_pDxCtx )
    {
        delete m_pDxCtx;
        m_DevInited = false;
    }
    m_pDxCtx = new DX11Context;

    // create dx11 device
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = (HWND)hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL currLevel = D3D_FEATURE_LEVEL_10_1;
    D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

    ID3D11Device* pDev = NULL;
    ID3D11DeviceContext* pCtx = NULL;
    HRESULT hr = D3D11CreateDeviceAndSwapChain( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, 
        featureLevels, 2, D3D11_SDK_VERSION, &sd, &m_pDxCtx->m_pSwapChain, &pDev, &currLevel, &pCtx );
    if( FAILED( hr ) )
        return false;

    m_pDxCtx->m_pDevice = pDev;
    m_pDxCtx->m_pImdCtx = pCtx;

    // create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = m_pDxCtx->m_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&pBackBuffer );
    if( FAILED( hr ) )
        return false;

    hr = pDev->CreateRenderTargetView( pBackBuffer, NULL, &m_pDxCtx->m_pRTView );
    pBackBuffer->Release();
    if( FAILED( hr ) )
        return false;
    pCtx->OMSetRenderTargets( 1, &m_pDxCtx->m_pRTView, NULL );

    // set viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    pCtx->RSSetViewports( 1, &vp );

     // create vertex shader
    ID3DBlob* pVSBlob = NULL;
    hr = CompileShaderFromFile( L"yuvShader11.fx", "VS", "vs_4_0", &pVSBlob );
    if( FAILED( hr ) )
        return false;

    hr = pDev->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pDxCtx->m_pVertexShader );
    if( FAILED( hr ) )
    {    
        pVSBlob->Release();
        return false;
    } 

    // set input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = pDev->CreateInputLayout( layout, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pDxCtx->m_pVertexLayout );
    pVSBlob->Release();
    if( FAILED( hr ) )
        return false;

    pCtx->IASetInputLayout( m_pDxCtx->m_pVertexLayout );

    // create pixel shader
    ID3DBlob* pPSBlob = NULL;
    hr = CompileShaderFromFile( L"yuvShader11.fx", "PS_BGR", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
        return false;
    hr = pDev->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pDxCtx->m_pBgrShader );
    pPSBlob->Release();
    if( FAILED( hr ) )
        return false;

    hr = CompileShaderFromFile( L"yuvShader11.fx", "PS_YUV709", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
        return false;
    hr = pDev->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pDxCtx->m_pYuv709Shader );
    pPSBlob->Release();
    if( FAILED( hr ) )
        return false;

    hr = CompileShaderFromFile( L"yuvShader11.fx", "PS_YUV601", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
        return false;
    hr = pDev->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pDxCtx->m_pYuv601Shader );
    pPSBlob->Release();
    if( FAILED( hr ) )
        return false;

    hr = CompileShaderFromFile( L"yuvShader11.fx", "PS_Luma", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
        return false;
    hr = pDev->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pDxCtx->m_pLumaShader );
    pPSBlob->Release();
    if( FAILED( hr ) )
        return false;

    // create vertex buffer
    D3D11_BUFFER_DESC bd;
    D3D11_SUBRESOURCE_DATA vertData;
    ZeroMemory( &bd, sizeof(bd) );
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof( s_Vertex );
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ZeroMemory( &vertData, sizeof(vertData) );
    vertData.pSysMem = s_Vertex;

    hr = pDev->CreateBuffer( &bd, &vertData, &m_pDxCtx->m_pVertexBuffer );
    if( FAILED( hr ) )
        return false;

    UINT stride = 20;
    UINT offset = 0;
    pCtx->IASetVertexBuffers( 0, 1, &m_pDxCtx->m_pVertexBuffer, &stride, &offset );
    pCtx->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    // create texture sampler
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory( &sampDesc, sizeof(sampDesc) );
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.MaxAnisotropy = 1;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0.f;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = pDev->CreateSamplerState( &sampDesc, &m_pDxCtx->m_pSampler );
    if( FAILED( hr ) )
        return false;

    // disable depth compare
    D3D11_DEPTH_STENCIL_DESC ddesc;
    ZeroMemory( &ddesc, sizeof(ddesc) );
    ddesc.DepthEnable = FALSE;
    ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ddesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    ddesc.StencilEnable = FALSE;
    ddesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    ddesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

    hr = pDev->CreateDepthStencilState( &ddesc, &m_pDxCtx->m_pDepthState );
    if( FAILED( hr ) )
        return false;
    pCtx->OMSetDepthStencilState( m_pDxCtx->m_pDepthState, 0 );

    // set vertex shader
    pCtx->VSSetShader( m_pDxCtx->m_pVertexShader, NULL, 0 );

    m_hwnd = hwnd;
    m_WinWidth = width;
    m_WinHeight = height;
    m_FrameWidth = 0;
    m_FrameHeight = 0;
    m_LumaWidth = 0;
    m_LumaHeight = 0;
    memset( &m_LastFrame, 0, sizeof( m_LastFrame ) );
    m_DevInited = true;

    return true;
}

void DX11Device::CloseDevice()
{
    delete m_pDxCtx;
    m_pDxCtx = NULL;
    m_DevInited = false;
}

void DX11Device::Present()
{
    if( m_pDxCtx && m_DevInited )
    {
        m_pDxCtx->m_pSwapChain->Present( 0, 0 );
    }
}

bool DX11Device::DrawBGRA( const uint8_t* pData, int width, int height, int pitch, DxRect* pRect )
{
    if( !m_DevInited )
        return false;

    HRESULT hr = S_OK;
    if( width != m_FrameWidth || height != m_FrameHeight )
    {
        SAFE_RELEASE( m_pDxCtx->m_pFrameTexView );
        SAFE_RELEASE( m_pDxCtx->m_pFrameTex );

        // create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof(desc) );
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        hr = m_pDxCtx->m_pDevice->CreateTexture2D( &desc, NULL, &m_pDxCtx->m_pFrameTex );
        if( FAILED( hr ) )
            return false;
        
        // create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC vdesc;
        ZeroMemory( &vdesc, sizeof(vdesc) );
        vdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        vdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        vdesc.Texture2D.MostDetailedMip = 0;
        vdesc.Texture2D.MipLevels = 1;
        hr = m_pDxCtx->m_pDevice->CreateShaderResourceView( m_pDxCtx->m_pFrameTex, &vdesc, &m_pDxCtx->m_pFrameTexView );
        if( FAILED( hr ) )
            return false;

        m_FrameWidth = width;
        m_FrameHeight = height;
    }

    ID3D11DeviceContext* pCtx = m_pDxCtx->m_pImdCtx;

    // update RGBA texture
    D3D11_MAPPED_SUBRESOURCE d3drc;
    hr = pCtx->Map( m_pDxCtx->m_pFrameTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc );
    if( SUCCEEDED( hr ) )
    {
        const uint8_t* src = pData;
        uint8_t* dst = (uint8_t*)d3drc.pData;
        for( int i = 0; i < height; i++ )
        {
            memcpy( dst, src, width * 4 );
            dst += d3drc.RowPitch;
            src += pitch;
        }

        pCtx->Unmap( m_pDxCtx->m_pFrameTex, 0 );
    }

    // update vertex buffer
    if( SUCCEEDED( pCtx->Map( m_pDxCtx->m_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc ) ) )
    {
        s_Vertex[3] = pRect ? (float)pRect->left / width : 0.f;
        s_Vertex[4] = pRect ? (float)pRect->bottom / height : 1.f;
        s_Vertex[8] = s_Vertex[3];
        s_Vertex[9] = pRect ? (float)pRect->top / height : 0.f;
        s_Vertex[13] = pRect ? (float)pRect->right / width : 1.f;
        s_Vertex[14] = s_Vertex[4];
        s_Vertex[18] = s_Vertex[13];
        s_Vertex[19] = s_Vertex[9];

        memcpy( d3drc.pData, s_Vertex, sizeof( s_Vertex ) );
        pCtx->Unmap( m_pDxCtx->m_pVertexBuffer, 0 );
    }

    // set viewport
    D3D11_VIEWPORT vp;
    if( m_WinWidth <= width )
    {
        vp.TopLeftX = 0;
        vp.Width = (float)m_WinWidth;
    }
    else
    {
        vp.TopLeftX = (float)((m_WinWidth - width) >> 1);
        vp.Width = (float)width;
    }
    if( m_WinHeight <= height )
    {
        vp.TopLeftY = 0;
        vp.Height = (float)m_WinHeight;
    }
    else
    {
        vp.TopLeftY = (float)((m_WinHeight - height) >> 1);
        vp.Height = (float)height;
    }
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    pCtx->RSSetViewports( 1, &vp );

    // set pixel shader and texture
    pCtx->PSSetShader( m_pDxCtx->m_pBgrShader, NULL, 0 );
    pCtx->PSSetShaderResources( 0, 1, &m_pDxCtx->m_pFrameTexView );
    pCtx->PSSetSamplers( 0, 1, &m_pDxCtx->m_pSampler );

    // render
    float black[4] = { 0.f, 0.f, 0.f, 1.0f };
    pCtx->ClearRenderTargetView( m_pDxCtx->m_pRTView, black );
    pCtx->Draw( 4, 0 );
    m_pDxCtx->m_pSwapChain->Present( 0, 0 );

    return true;
}

bool DX11Device::DrawLuma( const uint8_t* pData, int width, int height, int pitch, DxRect* pRect )
{
    if( !m_DevInited )
        return false;

    HRESULT hr = S_OK;
    if( width != m_LumaWidth || height != m_LumaHeight )
    {
        SAFE_RELEASE( m_pDxCtx->m_pLumaTexView );
        SAFE_RELEASE( m_pDxCtx->m_pLumaTex );

        // create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof(desc) );
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        hr = m_pDxCtx->m_pDevice->CreateTexture2D( &desc, NULL, &m_pDxCtx->m_pLumaTex );
        if( FAILED( hr ) )
            return false;

        // create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC vdesc;
        ZeroMemory( &vdesc, sizeof(vdesc) );
        vdesc.Format = DXGI_FORMAT_R8_UNORM;
        vdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        vdesc.Texture2D.MostDetailedMip = 0;
        vdesc.Texture2D.MipLevels = 1;
        hr = m_pDxCtx->m_pDevice->CreateShaderResourceView( m_pDxCtx->m_pLumaTex, &vdesc, &m_pDxCtx->m_pLumaTexView );
        if( FAILED( hr ) )
            return false;

        m_LumaWidth = width;
        m_LumaHeight = height;
    }

    ID3D11DeviceContext* pCtx = m_pDxCtx->m_pImdCtx;

    // update luma texture
    D3D11_MAPPED_SUBRESOURCE d3drc;
    hr = pCtx->Map( m_pDxCtx->m_pLumaTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc );
    if( SUCCEEDED( hr ) )
    {
        const uint8_t* src = pData;
        uint8_t* dst = (uint8_t*)d3drc.pData;
        for( int i = 0; i < height; i++ )
        {
            memcpy( dst, src, width );
            dst += d3drc.RowPitch;
            src += pitch;
        }

        pCtx->Unmap( m_pDxCtx->m_pLumaTex, 0 );
    }

    // update vertex buffer
    if( SUCCEEDED( pCtx->Map( m_pDxCtx->m_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc ) ) )
    {
        s_Vertex[3] = pRect ? (float)pRect->left / width : 0.f;
        s_Vertex[4] = pRect ? (float)pRect->bottom / height : 1.f;
        s_Vertex[8] = s_Vertex[3];
        s_Vertex[9] = pRect ? (float)pRect->top / height : 0.f;
        s_Vertex[13] = pRect ? (float)pRect->right / width : 1.f;
        s_Vertex[14] = s_Vertex[4];
        s_Vertex[18] = s_Vertex[13];
        s_Vertex[19] = s_Vertex[9];

        memcpy( d3drc.pData, s_Vertex, sizeof( s_Vertex ) );
        pCtx->Unmap( m_pDxCtx->m_pVertexBuffer, 0 );
    }

    // set viewport
    D3D11_VIEWPORT vp;
    if( m_WinWidth <= width )
    {
        vp.TopLeftX = 0;
        vp.Width = (float)m_WinWidth;
    }
    else
    {
        vp.TopLeftX = (float)((m_WinWidth - width) >> 1);
        vp.Width = (float)width;
    }
    if( m_WinHeight <= height )
    {
        vp.TopLeftY = 0;
        vp.Height = (float)m_WinHeight;
    }
    else
    {
        vp.TopLeftY = (float)((m_WinHeight - height) >> 1);
        vp.Height = (float)height;
    }
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    pCtx->RSSetViewports( 1, &vp );

    // set pixel shader
    pCtx->PSSetShader( m_pDxCtx->m_pLumaShader, NULL, 0 );
    pCtx->PSSetShaderResources( 0, 1, &m_pDxCtx->m_pLumaTexView );
    pCtx->PSSetSamplers( 0, 1, &m_pDxCtx->m_pSampler );

    // render
    float black[4] = { 0.f, 0.f, 0.f, 1.0f };
    pCtx->ClearRenderTargetView( m_pDxCtx->m_pRTView, black );
    pCtx->Draw( 4, 0 );
    m_pDxCtx->m_pSwapChain->Present( 0, 0 );

    return true;
}

bool DX11Device::DrawYUVP( const YUVFrame* pFrame, DxRect* pRect )
{
    if( !m_DevInited )
        return false;

    ID3D11Device* pDev = m_pDxCtx->m_pDevice;
    ID3D11DeviceContext* pCtx = m_pDxCtx->m_pImdCtx;
    const int width = pFrame->width[0];
    const int height = pFrame->height[0];

    if( m_LastFrame.width[0] != width || m_LastFrame.height[0] != height )
    {
        SAFE_RELEASE( m_pDxCtx->m_pYTexView );
        SAFE_RELEASE( m_pDxCtx->m_pYTex );

        // create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof(desc) );
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        HRESULT hr = pDev->CreateTexture2D( &desc, NULL, &m_pDxCtx->m_pYTex );
        if( FAILED( hr ) )
            return false;

        // create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC vdesc;
        ZeroMemory( &vdesc, sizeof(vdesc) );
        vdesc.Format = DXGI_FORMAT_R8_UNORM;
        vdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        vdesc.Texture2D.MostDetailedMip = 0;
        vdesc.Texture2D.MipLevels = 1;
        hr = pDev->CreateShaderResourceView( m_pDxCtx->m_pYTex, &vdesc, &m_pDxCtx->m_pYTexView );
        if( FAILED( hr ) )
            return false;

        m_LastFrame.width[0] = width;
        m_LastFrame.height[0] = height;
    }
    if( m_LastFrame.width[1] != pFrame->width[1] || m_LastFrame.height[1] != pFrame->height[1] )
    {
        SAFE_RELEASE( m_pDxCtx->m_pUTexView );
        SAFE_RELEASE( m_pDxCtx->m_pUTex );

        // create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof(desc) );
        desc.Width = pFrame->width[1];
        desc.Height = pFrame->height[1];
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        HRESULT hr = pDev->CreateTexture2D( &desc, NULL, &m_pDxCtx->m_pUTex );
        if( FAILED( hr ) )
            return false;

        // create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC vdesc;
        ZeroMemory( &vdesc, sizeof(vdesc) );
        vdesc.Format = DXGI_FORMAT_R8_UNORM;
        vdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        vdesc.Texture2D.MostDetailedMip = 0;
        vdesc.Texture2D.MipLevels = 1;
        hr = pDev->CreateShaderResourceView( m_pDxCtx->m_pUTex, &vdesc, &m_pDxCtx->m_pUTexView );
        if( FAILED( hr ) )
            return false;

        m_LastFrame.width[1] = pFrame->width[1];
        m_LastFrame.height[1] = pFrame->height[1];
    }
    if( m_LastFrame.width[2] != pFrame->width[2] || m_LastFrame.height[2] != pFrame->height[2] )
    {
        SAFE_RELEASE( m_pDxCtx->m_pVTexView );
        SAFE_RELEASE( m_pDxCtx->m_pVTex );

        // create texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof(desc) );
        desc.Width = pFrame->width[2];
        desc.Height = pFrame->height[2];
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        HRESULT hr = pDev->CreateTexture2D( &desc, NULL, &m_pDxCtx->m_pVTex );
        if( FAILED( hr ) )
            return false;

        // create texture view
        D3D11_SHADER_RESOURCE_VIEW_DESC vdesc;
        ZeroMemory( &vdesc, sizeof(vdesc) );
        vdesc.Format = DXGI_FORMAT_R8_UNORM;
        vdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        vdesc.Texture2D.MostDetailedMip = 0;
        vdesc.Texture2D.MipLevels = 1;
        hr = pDev->CreateShaderResourceView( m_pDxCtx->m_pVTex, &vdesc, &m_pDxCtx->m_pVTexView );
        if( FAILED( hr ) )
            return false;

        m_LastFrame.width[2] = pFrame->width[2];
        m_LastFrame.height[2] = pFrame->height[2];
    }

    // update YUV plane
    D3D11_MAPPED_SUBRESOURCE d3drc;
    if( SUCCEEDED( pCtx->Map( m_pDxCtx->m_pYTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc ) ) )
    {
        const uint8_t* src = pFrame->planes[0];
        uint8_t* dst = (uint8_t*)d3drc.pData;
        for( int i = 0; i < pFrame->height[0]; i++ )
        {
            memcpy( dst, src, pFrame->width[0] );
            dst += d3drc.RowPitch;
            src += pFrame->pitch[0];
        }

        pCtx->Unmap( m_pDxCtx->m_pYTex, 0 );
    }
    if( SUCCEEDED( pCtx->Map( m_pDxCtx->m_pUTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc ) ) )
    {
        const uint8_t* src = pFrame->planes[1];
        uint8_t* dst = (uint8_t*)d3drc.pData;
        for( int i = 0; i < pFrame->height[1]; i++ )
        {
            memcpy( dst, src, pFrame->width[1] );
            dst += d3drc.RowPitch;
            src += pFrame->pitch[1];
        }

        pCtx->Unmap( m_pDxCtx->m_pUTex, 0 );
    }
    if( SUCCEEDED( pCtx->Map( m_pDxCtx->m_pVTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc ) ) )
    {
        const uint8_t* src = pFrame->planes[2];
        uint8_t* dst = (uint8_t*)d3drc.pData;
        for( int i = 0; i < pFrame->height[2]; i++ )
        {
            memcpy( dst, src, pFrame->width[2] );
            dst += d3drc.RowPitch;
            src += pFrame->pitch[2];
        }

        pCtx->Unmap( m_pDxCtx->m_pVTex, 0 );
    }

    // update vertex buffer
    if( SUCCEEDED( pCtx->Map( m_pDxCtx->m_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3drc ) ) )
    {
        s_Vertex[3] = pRect ? (float)pRect->left / width : 0.f;
        s_Vertex[4] = pRect ? (float)pRect->bottom / height : 1.f;
        s_Vertex[8] = s_Vertex[3];
        s_Vertex[9] = pRect ? (float)pRect->top / height : 0.f;
        s_Vertex[13] = pRect ? (float)pRect->right / width : 1.f;
        s_Vertex[14] = s_Vertex[4];
        s_Vertex[18] = s_Vertex[13];
        s_Vertex[19] = s_Vertex[9];

        memcpy( d3drc.pData, s_Vertex, sizeof( s_Vertex ) );
        pCtx->Unmap( m_pDxCtx->m_pVertexBuffer, 0 );
    }

    // set viewport
    D3D11_VIEWPORT vp;
    if( m_WinWidth <= width )
    {
        vp.TopLeftX = 0;
        vp.Width = (float)m_WinWidth;
    }
    else
    {
        vp.TopLeftX = (float)((m_WinWidth - width) >> 1);
        vp.Width = (float)width;
    }
    if( m_WinHeight <= height )
    {
        vp.TopLeftY = 0;
        vp.Height = (float)m_WinHeight;
    }
    else
    {
        vp.TopLeftY = (float)((m_WinHeight - height) >> 1);
        vp.Height = (float)height;
    }
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    pCtx->RSSetViewports( 1, &vp );

    // set pixel shader
    ID3D11ShaderResourceView* yuvTexs[3] = { m_pDxCtx->m_pYTexView, m_pDxCtx->m_pUTexView, m_pDxCtx->m_pVTexView };
    if( pFrame->height[0] < 720 )
        pCtx->PSSetShader( m_pDxCtx->m_pYuv601Shader, NULL, 0 );
    else
        pCtx->PSSetShader( m_pDxCtx->m_pYuv709Shader, NULL, 0 );
    pCtx->PSSetShaderResources( 0, 3, yuvTexs );
    pCtx->PSSetSamplers( 0, 1, &m_pDxCtx->m_pSampler );

    // render
    float black[4] = { 0.f, 0.f, 0.f, 1.0f };
    pCtx->ClearRenderTargetView( m_pDxCtx->m_pRTView, black );
    pCtx->Draw( 4, 0 );
    m_pDxCtx->m_pSwapChain->Present( 0, 0 );

    return true;
}

#endif