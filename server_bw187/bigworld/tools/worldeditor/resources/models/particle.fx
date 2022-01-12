
technique EffectOverride
{
	pass Pass_0
	{
		ZEnable = True;
		ColorWriteEnable = RED | GREEN | BLUE;

		AlphaOp[0] = DISABLE;
		ColorOp[0] = DISABLE;

		PixelShader = NULL;
	}
}

