#include "stdinclude.fxh"

float4x4    worldViewProjection;
float       vOffset1;
float       vOffset2;

texture     patrolTexture;

struct VS_INPUT
{
    float4 pos		: POSITION;
    float3 normal   : NORMAL;
    float2 tex1     : TEXCOORD0;
    float2 tex2     : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 pos		: POSITION;
    float2 tex1     : TEXCOORD0;
    float2 tex2     : TEXCOORD1;
};

VS_OUTPUT vs_main(const VS_INPUT v)
{
	VS_OUTPUT o = (VS_OUTPUT)0;
    o.pos    = mul(v.pos, worldViewProjection);
    o.tex1   = v.tex1;
    o.tex2   = v.tex2;
    o.tex1.y += vOffset1;
    o.tex2.y += vOffset2;
	return o;
};

technique standard
{
	pass Pass_0
	{
		VertexShader = compile vs_1_1 vs_main();
		PixelShader  = NULL; 
		
	    ALPHABLENDENABLE = FALSE    ;
	    SRCBLEND         = ONE      ;
        FOGENABLE        = FALSE    ;
        LIGHTING         = FALSE    ;
        ZENABLE          = TRUE     ;
        ZFUNC            = LESSEQUAL;
        ZWRITEENABLE     = TRUE     ;
        CULLMODE         = NONE     ;
        TextureFactor    = 0xffffffff;
        
        COLOROP  [0]     = SELECTARG1;
        COLORARG1[0]     = TEXTURE ;
        COLORARG2[0]     = DIFFUSE ;
        ALPHAOP  [0]     = DISABLE ;      
        Texture  [0]     = (patrolTexture);
        ADDRESSU [0]     = WRAP;
        ADDRESSV [0]     = WRAP;
        TexCoordIndex[0] = 0;
        
        COLOROP  [1]     = MODULATE;
        COLORARG1[1]     = TEXTURE ;
        COLORARG2[1]     = CURRENT ;
        ALPHAOP  [1]     = DISABLE ;
        Texture  [1]     = (patrolTexture);
        ADDRESSU [1]     = WRAP;
        ADDRESSV [1]     = WRAP;  
        TexCoordIndex[1] = 1;      
        
        BW_TEXTURESTAGE_TERMINATE(2)              
	}
}

