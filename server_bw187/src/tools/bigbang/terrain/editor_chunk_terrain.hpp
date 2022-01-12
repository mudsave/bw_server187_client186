/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_TERRAIN_HPP_
#define EDITOR_CHUNK_TERRAIN_HPP_

#include "cstdmf/stringmap.hpp"
#include "chunk/chunk_terrain.hpp"
#include "moo/device_callback.hpp"
#include "moo/terrain_block.hpp"
#include "pyscript/script.hpp"

const float MAX_TERRAIN_SHADOW_RANGE = 500.f;

class EditorChunkTerrain;

class EditorTerrainBlock : public Moo::TerrainBlock
{
	typedef EditorTerrainBlock This;
public:
	bool save( const std::string& resourceName );
	bool saveCData( const std::string& resourceName );
	bool saveLegacy( const std::string& resourceName );

	std::vector<Moo::BaseTexturePtr>& textures()	{ return textures_; }
	std::vector<uint32>& blendValues()				{ return blendValues_; }
	std::vector<bool>& holes()						{ return holes_; }
	std::vector<uint8>&  detailIDs()				{ return detailIDs_; }

	const std::string& textureName( int level );
	void textureName( int level, const std::string& newName );
	void drawIgnoringHoles( bool setTextures = true );

	std::string resourceID_;

	virtual void draw( Moo::TerrainTextureSetter* tts );
	void resizeDetailIDs( int w, int h );

	PY_MODULE_STATIC_METHOD_DECLARE( py_createBlankTerrainFile )

	EditorChunkTerrain* chunkTerrain_;
	friend class EditorChunkTerrain;
};

typedef SmartPointer<EditorTerrainBlock>	EditorTerrainBlockPtr;

/**
 *	This class is the editor version of a 
 *	chunk item for a terrain block
 */
class EditorChunkTerrain : public ChunkTerrain
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkTerrain )
public:
	EditorChunkTerrain();

	EditorTerrainBlock&	block()	{ return static_cast<EditorTerrainBlock&>(*block_); }

	//Call this if you have changed the dimensions of the chunk
	void onAlterDimensions();

	//Helper functions
	void relativeElevations( std::vector<float>& returnedValues, int& w, int& h );
	float slope( int xIdx, int zIdx );

	void calculateShadows(bool canExitEarly = true);

	virtual const Matrix & edTransform()	{ return transform_; }
	virtual bool edTransform( const Matrix & m, bool transient );
	virtual bool edEdit( class ChunkItemEditor & editor );
	virtual void edBounds( BoundingBox& bbRet ) const;
	virtual bool edSave( DataSectionPtr pSection );
	virtual DataSectionPtr pOwnSect()	{ return pOwnSect_; }
	virtual const DataSectionPtr pOwnSect()	const { return pOwnSect_; }
	virtual void edPostClone( EditorChunkItem* srcItem );
	virtual BinaryPtr edExportBinaryData();
	virtual bool edShouldDraw();
	virtual Vector3 edMovementDeltaSnaps();
	virtual float edAngleSnaps();


	virtual void draw();

protected:
	virtual void toss( Chunk * pChunk );

private:
	Matrix transform_;
	DataSectionPtr pOwnSect_;

	bool load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString = NULL );
	static Moo::TerrainBlockPtr loadBlock( const std::string& resourceName, EditorChunkTerrain* chunkTerrain );

	static bool s_shouldDraw_;
	static uint32 s_settingsMark_;


	friend class EditorChunkTerrainProjector;
};

typedef SmartPointer<EditorChunkTerrain>	EditorChunkTerrainPtr;
typedef std::vector<EditorChunkTerrainPtr>	EditorChunkTerrainPtrs;


class EditorChunkTerrainCache
{
public:
	static EditorChunkTerrainCache& instance();

	EditorChunkTerrainPtr findChunkFromPoint( const Vector3& pos );
	int	findChunksWithinBox(
		const BoundingBox& bb,
		EditorChunkTerrainPtrs& outVector );

	void add( const std::string& name, EditorChunkTerrainPtr pChunkItem );
	void del( const std::string& name );
private:
	const std::string& chunkName( const Vector3& pos );
	
	EditorChunkTerrainCache();

	typedef StringHashMap<EditorChunkTerrainPtr> EditorTerrainChunkMap;
	EditorTerrainChunkMap	terrainChunks_;

	SimpleMutex terrainChunksMutex_;
};



/**
 * This editor-only class implements an index
 * buffer for a full terrain block ( ignoring holes )
 *
 * This singleton manages a single index buffer for all
 * terrain blocks with the same dimensions.  In general,
 * this means there will only ever be one.
 */
class TerrainBlockIndicesNoHoles : public Moo::DeviceCallback
{
public:
	static TerrainBlockIndicesNoHoles& instance();
	uint32	setIndexBuffer( uint32 blocksHeight, uint32 blocksWidth, uint32 verticesWidth );
	void	createManagedObjects();
	void	deleteManagedObjects();
private:
	//Key for the map
	class Dimensions
	{
	public:
		Dimensions( uint32 blocksHeight, uint32 blocksWidth, uint32 verticesWidth ):
			blocksHeight_(blocksHeight),
			blocksWidth_(blocksWidth),
			verticesWidth_(verticesWidth)
		{};
		bool operator <( const Dimensions & other ) const
			{ return (
				(blocksHeight_) < (other.blocksHeight_) ||
				(blocksWidth_) < (other.blocksWidth_) );
			}
		uint32 blocksHeight_;
		uint32 blocksWidth_;
		uint32 verticesWidth_;
	};

	//Index buffer and info
	class Indices
	{
	public:
		Indices(const Dimensions& d);
		void createManagedObjects();
		void deleteManagedObjects();
		uint32 setIndexBuffer();
		Moo::IndexBuffer	indexBuffer_;
		Dimensions dimensions_;
		uint32 nIndices_;
		uint32 nPrimitives_;
	};

	typedef std::map<Dimensions,Indices*> IndicesMap;
	IndicesMap indices_;

	TerrainBlockIndicesNoHoles();
	~TerrainBlockIndicesNoHoles();
};


#endif