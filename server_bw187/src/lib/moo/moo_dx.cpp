/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"

#include <map>
#include "moo_dx.hpp"

typedef std::map<D3DFORMAT, float> FormatBpp;

static FormatBpp formatBpp;
uint32	DX::surfaceSize( const D3DSURFACE_DESC& desc )
{
	if (!formatBpp.size())
	{
		formatBpp[D3DFMT_R8G8B8] = 3.f;
		formatBpp[D3DFMT_A8R8G8B8] = 4.f;
		formatBpp[D3DFMT_X8R8G8B8] = 4.f;
		formatBpp[D3DFMT_R5G6B5] = 2.f;
		formatBpp[D3DFMT_X1R5G5B5] = 2.f;
		formatBpp[D3DFMT_A1R5G5B5] = 2.f;
		formatBpp[D3DFMT_A4R4G4B4] = 2.f;
		formatBpp[D3DFMT_R3G3B2] = 1.f;
		formatBpp[D3DFMT_A8] = 1.f;
		formatBpp[D3DFMT_A8R3G3B2] = 2.f;
		formatBpp[D3DFMT_X4R4G4B4] = 2.f;
		formatBpp[D3DFMT_A2B10G10R10] = 4.f;
		formatBpp[D3DFMT_A8B8G8R8] = 4.f;
		formatBpp[D3DFMT_X8B8G8R8] = 4.f;
		formatBpp[D3DFMT_G16R16] = 4.f;
		formatBpp[D3DFMT_A2R10G10B10] = 4.f;
		formatBpp[D3DFMT_A16B16G16R16] = 8.f;
		formatBpp[D3DFMT_A8P8] = 2.f;
		formatBpp[D3DFMT_P8] = 1.f;
		formatBpp[D3DFMT_L8] = 1.f;
		formatBpp[D3DFMT_A8L8] = 2.f;
		formatBpp[D3DFMT_A4L4] = 1.f;
		formatBpp[D3DFMT_V8U8] = 2.f;
		formatBpp[D3DFMT_L6V5U5] = 2.f;
		formatBpp[D3DFMT_X8L8V8U8] = 4.f;
		formatBpp[D3DFMT_Q8W8V8U8] = 4.f;
		formatBpp[D3DFMT_V16U16] = 4.f;
		formatBpp[D3DFMT_A2W10V10U10] = 4.f;
		formatBpp[D3DFMT_UYVY] = 1.f;
		formatBpp[D3DFMT_R8G8_B8G8] = 2.f;
		formatBpp[D3DFMT_YUY2] = 1.f;
		formatBpp[D3DFMT_G8R8_G8B8] = 2.f;
		formatBpp[D3DFMT_DXT1] = 0.5f;
		formatBpp[D3DFMT_DXT2] = 1.f;
		formatBpp[D3DFMT_DXT3] = 1.f;
		formatBpp[D3DFMT_DXT4] = 1.f;
		formatBpp[D3DFMT_DXT5] = 1.f;
		formatBpp[D3DFMT_D16_LOCKABLE] = 2.f;
		formatBpp[D3DFMT_D32] = 4.f;
		formatBpp[D3DFMT_D15S1] = 2.f;
		formatBpp[D3DFMT_D24S8] = 4.f;
		formatBpp[D3DFMT_D24X8] = 4.f;
		formatBpp[D3DFMT_D24X4S4] = 4.f;
		formatBpp[D3DFMT_D16] = 2.f;
		formatBpp[D3DFMT_D32F_LOCKABLE] = 4.f;
		formatBpp[D3DFMT_D24FS8] = 4.f;
		formatBpp[D3DFMT_L16] = 2.f;
		formatBpp[D3DFMT_Q16W16V16U16] = 8.f;
		formatBpp[D3DFMT_MULTI2_ARGB8] = 0.f;
		formatBpp[D3DFMT_R16F] = 2.f;
		formatBpp[D3DFMT_G16R16F] = 4.f;
		formatBpp[D3DFMT_A16B16G16R16F] = 8.f;
		formatBpp[D3DFMT_R32F] = 4.f;
		formatBpp[D3DFMT_G32R32F] = 8.f;
		formatBpp[D3DFMT_A32B32G32R32F] = 16.f;
		formatBpp[D3DFMT_CxV8U8] = 2.f;
	}

	uint32 ret = 0;

	FormatBpp::iterator it = formatBpp.find( desc.Format );
	if (it != formatBpp.end())
	{
		ret = uint32(it->second * (float)(desc.Width * desc.Height));
	}
	return ret;
}
