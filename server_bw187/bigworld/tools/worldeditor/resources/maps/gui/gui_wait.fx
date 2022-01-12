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
		AlphaOp[0] = MODULATE;
		MagFilter[0] = LINEAR;
		ColorArg1[0] = CURRENT;
		ColorArg2[0] = TEXTURE;
		ColorOp[0] = MODULATE;
		AddressW[0] = WRAP;
		AddressV[0] = WRAP;
		Texture[0] = (diffuseMap);
		AddressU[0] = WRAP;
		AlphaArg1[0] = CURRENT;
		AlphaArg2[0] = TEXTURE;
		TexCoordIndex[0] = 0;

		AlphaOp[1] = DISABLE;
		ColorOp[1] = DISABLE;

		PixelShader = NULL;
	}
}

