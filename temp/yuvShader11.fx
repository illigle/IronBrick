Texture2D frameTex : register( t0 );
Texture2D uTex : register( t1 );
Texture2D vTex : register( t2 );
SamplerState texSam : register( s0 );

struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

// Simple Vertex Shader
PS_INPUT VS( VS_INPUT input )
{
	PS_INPUT outVert;
    outVert.Pos = input.Pos;
    outVert.Tex = input.Tex;
    return outVert;
}

// BGR Pixel Shader
float4 PS_BGR( PS_INPUT input ) : SV_Target
{
    float4 outPel = frameTex.Sample( texSam, input.Tex );
    return float4( outPel.b, outPel.g, outPel.r, 1.0 );
}

// ITU-R 709 Y : [16 235], Cb Cr : [16 240], alpha : [0 255]
// DirectX : [0 255]
float4 PS_YUV709( PS_INPUT input ) : SV_Target
{
	float4 outPel;
    float4 y4 = frameTex.Sample( texSam, input.Tex );
	float4 u4 = uTex.Sample( texSam, input.Tex );
    float4 v4 = vTex.Sample( texSam, input.Tex );
	
    float Y = (y4.r - 0.0627451) * 1.16438;
	float Cb = u4.r - 0.5;
	float Cr = v4.r - 0.5; 

	outPel.r = Y + 1.793 * Cr;
	outPel.g = Y - 0.534 * Cr - 0.213 * Cb;
	outPel.b = Y + 2.114 * Cb;
	outPel.a = 1.0;	
	
    return saturate( outPel );
}

// ITU-R 601 Y : [16 235], Cb Cr : [16 240], alpha : [0 255]
// DirectX : [0 255]
float4 PS_YUV601( PS_INPUT input ) : SV_Target
{
	float4 outPel;
    float4 y4 = frameTex.Sample( texSam, input.Tex );
	float4 u4 = uTex.Sample( texSam, input.Tex );
    float4 v4 = vTex.Sample( texSam, input.Tex );
	
    float Y = (y4.r - 0.0627451) * 1.16438;
	float Cb = u4.r - 0.5;
	float Cr = v4.r - 0.5; 

	outPel.r = Y + 1.596 * Cr;
	outPel.g = Y - 0.813 * Cr - 0.392 * Cb;
	outPel.b = Y + 2.017 * Cb;
	outPel.a = 1.0;	
	
    return saturate( outPel );
}

// render luma only
float4 PS_Luma( PS_INPUT input ) : SV_Target
{
    float4 outPel = frameTex.Sample( texSam, input.Tex );
    return float4( outPel.r, outPel.r, outPel.r, 1.0 );
}
