/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_MANAGER_HPP
#define CHUNK_MANAGER_HPP

#include <vector>
#include <map>

#include "cstdmf/aligned.hpp"
#include "cstdmf/smartpointer.hpp"
#include "chunk_boundary.hpp"

#include "moo/moo_math.hpp"

class Chunk;
class ChunkLoader;
class ChunkSpace;
class ChunkItem;
typedef uint32 ChunkSpaceID;
typedef SmartPointer<ChunkSpace> ChunkSpacePtr;
class ChunkDirMapping;

#include "umbra_config.hpp"

#if UMBRA_ENABLE
namespace Umbra
{
	class Camera;
	class Cell;
}
#endif

/**
 *	This singleton class manages most aspects of the chunky scene graph.
 *	It contains most of the API that classes outside the chunk library will
 *	need to use. It manages the universe that the game runs.
 *
 *	A universe defines the world for a whole game - both the client and
 *	the server run only one universe. Each universe is split up into
 *	a number of named spaces.
 */
class ChunkManager : public Aligned
{
public:
	ChunkManager();
	~ChunkManager();

	bool init();
	bool fini();

	// set the camera position
	void camera( const Matrix & cameraTransform, ChunkSpacePtr pSpace );

	// call everyone's tick method, plus scan for
	// new chunks to load and old chunks to dispose
	void tick( float dTime );

	// draw the scene from the set camera position
	void draw();

#if UMBRA_ENABLE
	void umbraDraw();
	void umbraRepeat();
#endif

	// add/del fringe chunks from this draw call
	void addFringe( Chunk * pChunk );
	void delFringe( Chunk * pChunk );

	// append the given chunk to the load list
//	void loadChunkExplicitly( const std::string & identifier, int spaceID );
	void loadChunkExplicitly( const std::string & identifier,
		ChunkDirMapping * pMapping );

	Chunk* findChunkByName( const std::string & identifier, 
        ChunkDirMapping * pMapping, bool createIfNotFound = true );
	void loadChunkNow( Chunk* chunk );
	void loadChunkNow( const std::string & identifier, ChunkDirMapping * pMapping );

	// accessors
	ChunkSpacePtr space( ChunkSpaceID spaceID, bool createIfMissing = true );

	ChunkSpacePtr cameraSpace() const;
	Chunk * cameraChunk() const			{ return cameraChunk_; }
	void clearAllSpaces();
	void clearAllServerSpaces();

	bool busy() const			{ return !loadingChunks_.empty(); }

	float maxLoadPath() const	{ return maxLoadPath_; }
	float minEjectPath() const	{ return minEjectPath_; }
	void maxLoadPath( float v )		{ maxLoadPath_ = v; }
	void minEjectPath( float v )	{ minEjectPath_ = v; }
	void autoSetPathConstraints( float farPlane );
	float closestUnloadedChunk( ChunkSpacePtr pSpace ) const;

	void addSpace( ChunkSpace * pSpace );
	void delSpace( ChunkSpace * pSpace );

	static ChunkManager & instance();

	static void drawTreeBranch( Chunk * pChunk, const std::string & why );
	static void drawTreeReturn();
	static const std::string & drawTree();

	static int      s_chunksTraversed;
	static int      s_chunksVisible;
	static int      s_chunksReflected;
	static int      s_visibleCount;
	static int      s_drawPass;
	
	static bool     s_drawVisibilityBBoxes;
	static bool     s_enableChunkCulling;

	ChunkLoader * chunkLoader()	const	{ return pLoader_; }

	bool checkLoadingChunks();

	unsigned int workingInSyncMode_;
	void switchToSyncMode( bool sync );

#if UMBRA_ENABLE
	Umbra::Camera*	getUmbraCamera() { return umbraCamera_; }
	void			setUmbraCamera( Umbra::Camera* pCamera ) { umbraCamera_ = pCamera; }
#endif
	void addToCache(Chunk* pChunk, bool fringeOnly);
	void clearCache();
	
	Vector3 cameraNearPoint() const;
	Vector3 cameraAxis(int axis) const;
	
private:
	bool		initted_;

	typedef std::map<ChunkSpaceID,ChunkSpace*>	ChunkSpaces;
	ChunkSpaces			spaces_;

	ChunkLoader		* pLoader_;

	Matrix			cameraTrans_;
	ChunkSpacePtr	pCameraSpace_;
	Chunk			* cameraChunk_;

	typedef std::vector<Chunk*>		ChunkVector;
	ChunkVector		loadingChunks_;
	Chunk			* pFoundSeed_;
	Chunk			* fringeHead_;

	bool scan();
	bool blindpanic();
	bool autoBootstrapSeedChunk();

	void loadChunk( Chunk * pChunk, bool highPriority );

#if UMBRA_ENABLE
	void			checkCameraBoundaries();
#endif

	float			maxLoadPath_;	// bigger than sqrt(500^2 + 500^2)
	float			minEjectPath_;

	float			scanSkippedFor_;
	Vector3			cameraAtLastScan_;
	bool			noneLoadedAtLastScan_;

#if UMBRA_ENABLE
	// umbra
	Umbra::Camera*	umbraCamera_;
#endif
	
	friend class ChunkLoader;
};


/**
 *  This class is used to turn on/off sync mode of the ChunkManager in a 
 *  scoped fashion.
 */
class ScopedSyncMode
{
public:
    ScopedSyncMode();
    ~ScopedSyncMode();

private:
    ScopedSyncMode(const ScopedSyncMode &);             // not allowed
    ScopedSyncMode &operator=(const ScopedSyncMode &);  // not allowed
};


#endif // CHUNK_MANAGER_HPP
