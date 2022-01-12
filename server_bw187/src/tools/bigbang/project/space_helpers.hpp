/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

#include "chunk/base_chunk_space.hpp"
#include "chunk/chunk_space.hpp"
#include "../big_bang.hpp"
#include "grid_coord.hpp"

class HexLookup
{
public:
	HexLookup()
	{
		hexLookup_ = new uint16[256];
		hexFactor_ = new uint8[256];
		ZeroMemory(hexLookup_, sizeof(uint16)*256);
		ZeroMemory(hexFactor_, sizeof(uint8)*256);

		for (unsigned char i='0'; i<='9'; i++)
		{
			hexLookup_[i] = (uint16)(i-'0');
			hexFactor_[i] = 1;
		}

		for (unsigned char i='a'; i<='f'; i++)
		{
			hexLookup_[i] = (uint16)(i-'a' + 10);
			hexFactor_[i] = 1;
		}

		for (unsigned char i='A'; i<='F'; i++)
		{
			hexLookup_[i] = (uint16)(i-'A' + 10);
			hexFactor_[i] = 1;
		}
	};

	~HexLookup()
	{
		delete[] hexLookup_;
		delete[] hexFactor_;
	}

	//done this way to avoid conditionals.
	//ascii-hex numbers directly looked up, while the factors are multiplied.
	//if any ascii-hex lookup is invalid, one of the factors will be 0 and thus
	//multiplied out, all will be invalid.
	bool fromHex( const unsigned char* f, int16& value )
	{
		value = (hexLookup_[f[3]] + 
			(hexLookup_[f[2]]<<4) +
			(hexLookup_[f[1]]<<8) +
			(hexLookup_[f[0]]<<12));
		int8 factor = hexFactor_[f[0]] * hexFactor_[f[1]] * hexFactor_[f[2]] * hexFactor_[f[3]];
		return (!!factor);
	}

	uint16* hexLookup_;
	uint8* hexFactor_;
};


/**
 *	This method turns a biased grid coordinate ( 0 .. width, 0 .. height ) into
 *	a chunk-style grid coordinate ( min.x .. min.x + width )
 */
inline void offsetGrid( const GridCoord& localToWorld, uint16 x, uint16 z, int16& offsetX, int16& offsetZ )
{
	offsetX = x;
	offsetZ = z;
	offsetX += localToWorld.x;
	offsetZ += localToWorld.y;
}


/**
 *	This method turns an offset grid coordinate ( min.x .. min.x + width/2 ) into
 *	a biased (unsigned) grid coordinate ( 0..width, 0..height )
 */
inline void biasGrid( const GridCoord& localToWorld, int16 x, int16 z, uint16& biasedX, uint16& biasedZ )
{
	biasedX = x;
	biasedZ = z;
	biasedX -= localToWorld.x;
	biasedZ -= localToWorld.y;

	MF_ASSERT( biasedX<32768 )
	MF_ASSERT( biasedZ<32768 )
}


//can return false, in case we picked up a stray .thumbnail.dds file ( e.g.
//space.thumbnail.dds )
inline bool gridFromThumbnailFileName( const unsigned char* fileName, int16& x, int16& z )
{
	static HexLookup hexLookup;

	//Assume name is dir/path/path/.../chunkNameo.thumbnail.dds for speed
	//Assume name is dir/path/path/.../chunkNameo.cData for speed
	const unsigned char* f = fileName;
	while ( *f ) f++;

	//subtract "xxxxxxxxo.cdata" which is always the last part of an
	//outside chunk identifier.
	if (( f-fileName >= 15 ) && (*(f-7) == 'o'))
	{
		return (hexLookup.fromHex(f-15,x) && hexLookup.fromHex(f-11,z));
	}

	return false;
}


inline bool gridFromChunk( const Chunk* pChunk, int16& x, int16& z )
{
	static HexLookup hexLookup;

	const unsigned char * id = (const unsigned char *)pChunk->identifier().c_str();
	const unsigned char* f = id;
	while ( *f ) f++;

	//subtract "xxxxxxxxo.cdata" which is always the last part of an
	//outside chunk identifier.
	if ( f-id >= 9 )
	{
		return (hexLookup.fromHex(f-9,x) && hexLookup.fromHex(f-5,z));
	}

	return false;
}


inline void chunkID( std::string& retChunkName, int16 gridX, int16 gridZ )
{
	Vector3 localPos( (float)(gridX*GRID_RESOLUTION), 0.f, (float)(gridZ*GRID_RESOLUTION) );
	localPos.x += GRID_RESOLUTION/2.f;
	localPos.z += GRID_RESOLUTION/2.f;
	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
    if (dirMap != NULL)
	    retChunkName = dirMap->outsideChunkIdentifier(localPos);
    else
        retChunkName = std::string();
}


inline std::string thumbnailFilename( const std::string& pathName, const std::string& chunkName )
{
	return pathName + chunkName + ".cdata/thumbnail.dds";
}


inline bool thumbnailExists(  const std::string& pathName, const std::string& chunkName )
{
	DataSectionPtr pSection = BWResource::openSection( pathName + chunkName + ".cdata", false );
	if ( !pSection )
		return false;
	if ( !pSection->openSection( "thumbnail.dds" ) )
		return false;
	return true;
}