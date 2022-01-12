texture diffuseMap < bool artistEditable = true;  >;

technique EffectOverride
{
	pass Pass_0
	{
		TextureFactor = -1;
		ZWriteEnable = FALSE;
		AlphaRef = 0;
		SrcBlend = ONE;
		AlphaBlendEnable = TRUE;
		ColorWriteEnable = RED | GREEN | BLUE;
		DestBlend = ONE;
		AlphaTestEnable = FALSE;
		ZEnable = True;
		FogEnable = FALSE;
		ZFunc = LESSEQUAL;

		MipFilter[0] = NONE;
		MinFilter[0] = LINEAR;
		AlphaOp[0] = DISABLE;
		MagFilter[0] = LINEAR;
		ColorArg1[0] = TEXTURE;
		ColorArg2[0] = TFACTOR;
		ColorOp[0] = MODULATE;
		AddressW[0] = CLAMP;
		AddressV[0] = CLAMP;
		Texture[0] = (diffuseMap);
		AddressU[0] = CLAMP;
		AlphaArg1[0] = CURRENT;
		AlphaArg2[0] = TEXTURE;
		TexCoordIndex[0] = 0;

		AlphaOp[1] = DISABLE;
		ColorOp[1] = DISABLE;

		PixelShader = NULL;
	}
}

