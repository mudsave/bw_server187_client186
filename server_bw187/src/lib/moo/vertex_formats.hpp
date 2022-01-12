/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VERTEX_FORMATS_HPP
#define VERTEX_FORMATS_HPP

#include "moo_math.hpp"

#ifdef MF_SERVER
#define FVF( FORMAT )
#else
#define FVF( FORMAT ) static int fvf() { return FORMAT; }
#endif


namespace Moo
{

#pragma pack(push, 1 )
	inline Vector3 unpackNormal( uint32 packed )
	{
		int32 z = int32(packed) >> 22;
		int32 y = int32( packed << 10 ) >> 21;
		int32 x = int32( packed << 21 ) >> 21;

		return Vector3( float( x ) / 1023.f, float( y ) / 1023.f, float( z ) / 511.f );
	}

	struct VertexXYZNUV
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1 )
	};

	struct VertexXYZNUV2
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
        Vector2		uv2_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2 )
	};

	struct VertexXYZNDUV
	{
		Vector3		pos_;
		Vector3		normal_;
		DWORD		colour_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1 )
	};

	struct VertexXYZN
	{
		Vector3		pos_;
		Vector3		normal_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL )
	};

	struct VertexXYZND
	{
		Vector3		pos_;
		Vector3		normal_;
		DWORD		colour_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE )
	};

	struct VertexXYZNDS
	{
		Vector3		pos_;
		Vector3		normal_;
		DWORD		colour_;
		DWORD		specular_;

		FVF( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_SPECULAR )
	};

	struct VertexXYZL
	{
		Vector3		pos_;
		DWORD		colour_;

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE )
	};

	struct VertexXYZ
	{
		Vector3		pos_;

		FVF( D3DFVF_XYZ )
	};

	struct VertexXYZUV
	{
		Vector3		pos_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_TEX1 )
	};

	struct VertexXYZDUV
	{
		Vector3		pos_;
		DWORD		colour_;
		Vector2		uv_;

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1 )
	};

	struct VertexXYZDUV2 
	{ 
		Vector3     pos_; 
		DWORD       colour_; 
		Vector2     uv_; 
		Vector2     uv2_; 

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX2 ) 
	}; 

	struct VertexXYZUV2 
	{ 
		Vector3     pos_; 
		Vector2     uv_; 
		Vector2     uv2_; 

		FVF( D3DFVF_XYZ|D3DFVF_TEX2 ) 
	};
	struct VertexUV4 
	{ 
		Vector4     pos_; 
		Vector2     uv_[4]; 

		FVF( D3DFVF_XYZRHW|D3DFVF_TEX4 ) 
	}; 

	struct VertexNUV
	{
		Vector3		normal_;
		Vector2		uv_;

		FVF( D3DFVF_NORMAL|D3DFVF_TEX1 )
	};

	struct VertexN
	{
		Vector3		normal_;

		FVF( D3DFVF_NORMAL )
	};

	struct VertexUV
	{
		Vector2		uv_;

		FVF( D3DFVF_TEX1 )
	};

	struct VertexXYZNUVI
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		float		index_;
	};

	struct VertexYN
	{
		float		y_;
		Vector3		normal_;
	};

	struct VertexTL
	{
		Vector4		pos_;
		DWORD		colour_;

		FVF( D3DFVF_XYZRHW|D3DFVF_DIFFUSE )
	};

	struct VertexTUV
	{
		Vector4		pos_;
		Vector2		uv_;
		FVF( D3DFVF_XYZRHW|D3DFVF_TEX1 )
	};

	struct VertexTLUV
	{
		Vector4		pos_;
		DWORD		colour_;
		Vector2		uv_;
		FVF( D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1 )
	};

	struct VertexTDSUV2
	{
		Vector4		pos_;
		DWORD		colour_;
		DWORD		specular_;
		Vector2		uv_;
		Vector2		uv2_;

		FVF( D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX2 )
	};

	struct VertexXYZDSUV
	{
		float	x;
		float	y;
		float	z;
		uint32	colour;
		uint32	spec;
		float	tu;
		float	tv;

		FVF( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1 )
	};
	struct VertexTDSUVUUVV2
	{
		Vector4		pos_;
		DWORD		colour_;
		DWORD		specular_;
		Vector4		uvuuvv_;
		Vector4		uvuuvv2_;
	};

	struct VertexXYZNUVTB
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		uint32		tangent_;
		uint32		binormal_;
	};


	struct VertexXYZNUVTBPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		Vector3		tangent_;
		Vector3		binormal_;
		VertexXYZNUVTBPC& operator =(const VertexXYZNUVTB& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );
			return *this;
		}
		VertexXYZNUVTBPC(const VertexXYZNUVTB& in)
		{
			*this = in;
		}
		VertexXYZNUVTBPC& operator =(const VertexXYZNUV& in)
		{
			pos_ = in.pos_;
			normal_ = in.normal_;
			uv_ = in.uv_;
			tangent_.setZero();
			binormal_.setZero();
			return *this;
		}
		VertexXYZNUVTBPC(const VertexXYZNUV& in)
		{
			*this = in;
		}
		VertexXYZNUVTBPC()
		{
		}
	};

	struct VertexXYZNUVITB
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		float		index_;
		uint32		tangent_;
		uint32		binormal_;
	};

	struct VertexXYZNUVITBPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		float		index_;
		Vector3		tangent_;
		Vector3		binormal_;
		VertexXYZNUVITBPC& operator =(const VertexXYZNUVITB& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			index_ = in.index_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );
			return *this;
		}
		VertexXYZNUVITBPC(const VertexXYZNUVITB& in)
		{
			*this = in;
		}
		VertexXYZNUVITBPC()
		{
		}
	};

	struct VertexXYZNUVIIIWW
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		uint8		index_;
		uint8		index2_;
		uint8		index3_;
		uint8		weight_;
		uint8		weight2_;
	};

	struct VertexXYZNUVIIIWWTB
	{
		Vector3		pos_;
		uint32		normal_;
		Vector2		uv_;
		uint8		index_;
		uint8		index2_;
		uint8		index3_;
		uint8		weight_;
		uint8		weight2_;
		uint32		tangent_;
		uint32		binormal_;
	};

	struct VertexXYZNUVIIIWWPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		uint8		index3_;
		uint8		index2_;
		uint8		index_;
		uint8		pad_;
		uint8		pad2_;
		uint8		weight2_;
		uint8		weight_;
		uint8		pad3_;
		VertexXYZNUVIIIWWPC& operator =(const VertexXYZNUVIIIWW& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			weight_ = in.weight_;
			weight2_ = in.weight2_;
			index_ = in.index_;
			index2_ = in.index2_;
			index3_ = in.index3_;
			return *this;
		}
		VertexXYZNUVIIIWWPC(const VertexXYZNUVIIIWW& in)
		{
			*this = in;
		}
		VertexXYZNUVIIIWWPC()
		{
		}
	};

	struct VertexXYZNUVIIIWWTBPC
	{
		Vector3		pos_;
		Vector3		normal_;
		Vector2		uv_;
		uint8		index3_;
		uint8		index2_;
		uint8		index_;
		uint8		pad_;
		uint8		pad2_;
		uint8		weight2_;
		uint8		weight_;
		uint8		pad3_;
		Vector3		tangent_;
		Vector3		binormal_;
		VertexXYZNUVIIIWWTBPC& operator =(const VertexXYZNUVIIIWWTB& in)
		{
			pos_ = in.pos_;
			normal_ = unpackNormal( in.normal_ );
			uv_ = in.uv_;
			weight_ = in.weight_;
			weight2_ = in.weight2_;
			index_ = in.index_;
			index2_ = in.index2_;
			index3_ = in.index3_;
			tangent_ = unpackNormal( in.tangent_ );
			binormal_ = unpackNormal( in.binormal_ );
			return *this;
		}
		VertexXYZNUVIIIWWTBPC(const VertexXYZNUVIIIWWTB& in)
		{
			*this = in;
		}
		VertexXYZNUVIIIWWTBPC()
		{
		}
	};

	struct VertexYNDS
	{
		float		y_;
		uint32		normal_;
		uint32		diffuse_;
		uint16		shadow_;
	};

	struct VertexUVXB
	{
		int16		u_;
		int16		v_;
	};

	struct VertexXYZDP
	{
		Vector3		pos_;
		uint32		colour_;
		float		size_;

		FVF( D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_PSIZE )
	};
#pragma pack( pop )

}
#endif
