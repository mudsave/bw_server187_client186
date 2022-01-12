texture diffuseMap < bool artistEditable = true;  >;

technique EffectOverride
{
	pass Pass_0
	{
		TextureFactor = -1;
		ZWriteEnable = FALSE;
		AlphaRef = 0;
		CullMode = NONE;
		SrcBlend = ONE;
		AlphaBlendEnable = TRUE;
		ColorWriteEnable = RED | GREEN | BLUE;
		DestBlend = ONE;
		AlphaTestEnable = FALSE;
		ZEnable = True;
		ZFunc = LESSEQUAL;

		MipFilter[0] = LINEAR;
		MinFilter[0] = LINEAR;
		AlphaOp[0] = SELECTARG1;
		MagFilter[0] = LINEAR;
		ColorArg1[0] = TEXTURE;
		ColorArg2[0] = DIFFUSE;
		ColorOp[0] = MODULATE4X;
		AddressW[0] = MIRROR;
		AddressV[0] = MIRROR;
		Texture[0] = (diffuseMap);
		AddressU[0] = MIRROR;
		AlphaArg1[0] = TEXTURE;
		AlphaArg2[0] = DIFFUSE;
		TexCoordIndex[0] = 0;

		AlphaOp[1] = DISABLE;
		ColorOp[1] = DISABLE;

		PixelShader = NULL;
	}
}

