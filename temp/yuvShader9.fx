sampler YPlaneTex : register( s0 );           // Whatever texture is set using IDirect3DDevice9::SetTexture( 0, ... )
sampler UPlaneTex : register( s1 );
sampler VPlaneTex : register( s2 );

float4 PS_BGR( float2 tex : TEXCOORD0 ) : COLOR0
{ 
    float4 outPel = tex2D( YPlaneTex, tex );
    return float4( outPel.r, outPel.g, outPel.b, 1.0 );
}

// ITU-R 709 标准 Y : [16 235], Cb Cr : [16 240], alpha : [0 255]
// DirectX : [0 255]
float4 PS_YUV709( float2 tex : TEXCOORD0 ) : COLOR0
{ 
	float4 outPel;
    float4 y4 = tex2D( YPlaneTex, tex );
	float4 u4 = tex2D( UPlaneTex, tex );
    float4 v4 = tex2D( VPlaneTex, tex );
	
	float Y = (y4.r - 0.0627451) * 1.16438;
	float Cb = u4.r - 0.5;
	float Cr = v4.r - 0.5; 

	outPel.r = Y + 1.793 * Cr;
	outPel.g = Y - 0.534 * Cr - 0.213 * Cb;
	outPel.b = Y + 2.114 * Cb;
	outPel.a = 1.0;	
	
    return saturate( outPel );
}

// ITU-R 601 标准 Y : [16 235], Cb Cr : [16 240], alpha : [0 255]
// DirectX : [0 255]
float4 PS_YUV601( float2 tex : TEXCOORD0 ) : COLOR0
{ 
	float4 outPel;
    float4 y4 = tex2D( YPlaneTex, tex );
	float4 u4 = tex2D( UPlaneTex, tex );
    float4 v4 = tex2D( VPlaneTex, tex );
	
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
float4 PS_Luma( float2 tex : TEXCOORD0 ) : COLOR0
{ 
    float4 outPel = tex2D( YPlaneTex, tex );
    return float4( outPel.r, outPel.r, outPel.r, 1.0 );
}
