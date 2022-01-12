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

#include "script_bigworld.hpp"
#include "app.hpp"

#include "physics2/worldtri.hpp"

#include "pyscript/script.hpp"
#include "pyscript/script_math.hpp"
#include "pyscript/py_output_writer.hpp"
#include "pyscript/py_data_section.hpp"

#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk_overlapper.hpp"

#include "romp/weather.hpp"
#include "romp/xconsole.hpp"

#include "resmgr/xml_section.hpp"

#include "moo/texture_manager.hpp"
#include "moo/graphics_settings.hpp"
#include "moo/animation_manager.hpp"
#include "moo/visual_splitter.hpp"

#include "compile_time.hpp"

#include "entity_manager.hpp"
#include "entity_type.hpp"

#include "network/remote_stepper.hpp"

#include "cstdmf/debug.hpp"

#include <queue>

DECLARE_DEBUG_COMPONENT2( "Script", 0 )




// -----------------------------------------------------------------------------
// Section: BigWorld module: Chunk access functions
// -----------------------------------------------------------------------------


/*~ function BigWorld.weather
 *
 *	@param spaceID The id of the space to retrieve the weather object from
 *	@return	the unique Weather object.
 *
 *	This function returns the unique Weather object which is used to control
 *	the weather on the client.
 */
/**
 *	Returns the weather object.
 */
static PyObject * weather( SpaceID spaceID )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().space( spaceID, false );
	if (pSpace)
	{
		Weather * pWeather = pSpace->enviro().weather();
		if (pWeather != NULL)
		{
			Py_INCREF( pWeather );
			return pWeather;
		}
		else
		{
			PyErr_Format( PyExc_EnvironmentError, "BigWorld.weather(): "
				"No weather object in space ID %d", spaceID );
			return NULL;
		}
	}

	PyErr_Format( PyExc_ValueError, "BigWorld.weather(): "
		"No space ID %d", spaceID );
	return NULL;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, weather, ARG( SpaceID, END ), BigWorld )


typedef SmartPointer<PyObject> PyObjectPtr;

/**
 *	Helper class to store the triangle collided with
 */
class FDPTriangle : public CollisionCallback
{
public:
	WorldTriangle	tri_;

private:
	virtual int operator()( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float dist )
	{
		tri_ = WorldTriangle(
			obstacle.transform_.applyPoint( triangle.v0() ),
			obstacle.transform_.applyPoint( triangle.v1() ),
			obstacle.transform_.applyPoint( triangle.v2() ),
			triangle.flags() );

		return 1;	// see if there's anything closer...
	}
};

/*~ function BigWorld.findDropPoint
 *
 *	@param spaceID The ID of the space you want to do the raycast in
 *	@param vector3 Start point for the collision test
 *	@return A pair of the drop point, and the triangle it hit,
 *		or None if nothing was hit.
 *
 *	Finds the point directly beneath the start point that collides with
 *	the collision scene and terrain (if present in that location)
 */
/**
 *	Finds the drop point under the input point using the collision
 *	scene and terrain (if present in that location)
 *
 *	@return A pair of the drop point, and the triangle it hit,
 *		or None if nothing was hit.
 */
static PyObject * findDropPoint( SpaceID spaceID, const Vector3 & inPt )
{
	Vector3		outPt;
//	Vector3		triangle[3];

	ChunkSpacePtr pSpace = ChunkManager::instance().space( spaceID, false );
	if (!pSpace)
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.findDropPoint(): "
			"No space ID %d", spaceID );
		return NULL;
	}

	FDPTriangle fdpt;
	Vector3 ndPt( inPt.x, inPt.y-100.f, inPt.z );
	float dist = pSpace->collide( inPt, ndPt, fdpt );
	if (dist < 0.f)
	{
		Py_Return;
	}
	outPt.set( inPt.x, inPt.y-dist, inPt.z );
//	triangle[0] = fdpt.tri_.v0();
//	triangle[1] = fdpt.tri_.v1();
//	triangle[2] = fdpt.tri_.v2();

	PyObjectPtr results[4] = {
		PyObjectPtr( Script::getData( outPt ), true ),
		PyObjectPtr( Script::getData( fdpt.tri_.v0() ), true ),
		PyObjectPtr( Script::getData( fdpt.tri_.v1() ), true ),
		PyObjectPtr( Script::getData( fdpt.tri_.v2() ), true ) };
	return Py_BuildValue( "(O,(O,O,O))",
		&*results[0], &*results[1], &*results[2], &*results[3] );
}
PY_AUTO_MODULE_FUNCTION( RETOWN, findDropPoint,
	ARG( SpaceID, ARG( Vector3, END ) ), BigWorld )

static PyObject * collide( SpaceID spaceID, const Vector3 & src, const Vector3 & dst )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().space( spaceID, false );
	if (!pSpace)
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.collide(): "
			"No space ID %d", spaceID );
		return NULL;
	}

	FDPTriangle fdpt;
	float dist = pSpace->collide(
		src, dst, fdpt );
	if (dist < 0.f)
	{
		Py_Return;
	}

	Vector3 dir = dst-src;
	dir.normalise();
	Vector3 hitpt = src + dir * dist;

	PyObjectPtr results[4] = {
		PyObjectPtr( Script::getData( hitpt ), true ),
		PyObjectPtr( Script::getData( fdpt.tri_.v0() ), true ),
		PyObjectPtr( Script::getData( fdpt.tri_.v1() ), true ),
		PyObjectPtr( Script::getData( fdpt.tri_.v2() ), true ) };
	return Py_BuildValue( "(O,(O,O,O),i)",
		&*results[0], &*results[1], &*results[2], &*results[3], fdpt.tri_.materialKind() );
}
PY_AUTO_MODULE_FUNCTION( RETOWN, collide, ARG( SpaceID,
	ARG( Vector3, ARG( Vector3, END ) ) ), BigWorld )


/*~ function BigWorld.resetEntityManager
 *
 *	Resets the entity manager's state. After disconnecting, the
 *	entity will retain all of it's state. Allowing the world to
 *	still be explored offline, even with entity no longer being
 *	updated by the server. Calling this function resets it,
 *	deleting all entities and removing player and camera from
 *	the space.
 */
static void resetEntityManager()
{
	EntityManager::instance().disconnected();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, resetEntityManager, END, BigWorld )


/*~ function BigWorld.cameraSpaceID
 *
 *	@param spaceID (optional) Sets the space that the camera exists in.
 *	@return spaceID The spaceID that the camera currently exists in.
 *		If the camera is not in any space then 0 is returned.
 */
/**
 *	This function returns the id of the space that the camera is currently in.
 *	If the camera is not in any space then 0 is returned.
 *	You can optionally set the spaceID by passing it as an argument.
 */
static PyObject * cameraSpaceID( SpaceID newSpaceID = 0 )
{
	if (newSpaceID != 0)
	{
		ChunkSpacePtr pSpace =
			ChunkManager::instance().space( newSpaceID, false );
		if (!pSpace)
		{
			PyErr_Format( PyExc_ValueError, "BigWorld.cameraSpaceID: "
				"No such space ID %d\n", newSpaceID );
			return NULL;
		}

		ChunkManager::instance().camera( Moo::rc().invView(), pSpace );
	}
	else
	{
		ChunkManager::instance().camera( Moo::rc().invView(), NULL );
	}

	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	return Script::getData( pSpace ? pSpace->id() : 0 );
}
PY_AUTO_MODULE_FUNCTION( RETOWN, cameraSpaceID,
	OPTARG( SpaceID, 0, END ), BigWorld )


/// This is a vector of spaces created by the client
static std::vector<ChunkSpacePtr> s_clientSpaces;

/*~ function BigWorld.createSpace
 *
 * @return spaceID The ID of the client-only space that has been created
 *
 *	The space is held in a collection of client spaces for safe keeping,
 *	so that it is not prematurely disposed.
 */
/**
 *	This function creates and returns the id of a new client-only space.
 *
 *	The space is held in a collection of client spaces for safe keeping,
 *	so that it is not prematurely disposed.
 */
static SpaceID createSpace()
{
	static SpaceID clientSpaceID = (1 << 30);	// say

	ChunkManager & cm = ChunkManager::instance();

	SpaceID newSpaceID = 0;
	do
	{
		newSpaceID = clientSpaceID++;
		if (clientSpaceID == 0) clientSpaceID = (1 << 30);
	} while (cm.space( newSpaceID, false ));

	ChunkSpacePtr pNewSpace = cm.space( newSpaceID );
	s_clientSpaces.push_back( pNewSpace );
	return pNewSpace->id();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, createSpace, END, BigWorld )


/**
 *	This helper function gets the given client space ID.
 */
static ChunkSpacePtr getClientSpace( SpaceID spaceID, const char * methodName )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().space( spaceID, false );
	if (!pSpace)
	{
		PyErr_Format( PyExc_ValueError, "%s: No space ID %d",
			methodName, spaceID );
		return NULL;
	}

	if (std::find( s_clientSpaces.begin(), s_clientSpaces.end(), pSpace ) ==
		s_clientSpaces.end())
	{
		PyErr_Format( PyExc_ValueError, "%s: Space ID %d is not a client space "
			"(or is no longer held)", methodName, spaceID );
		return NULL;
	}

	return pSpace;
}


/*~ function BigWorld.releaseSpace
 *
 *	This function releases the given client space ID.
 *
 *	Note that the space will not necessarily be immediately deleted - that must
 *	wait until it is no longer referenced (including by chunk dir mappings).
 *
 *	Raises a ValueError if the space for the given spaceID is not found.
 *
 *	@param spaceID The ID of the space to release.
 *
 */
/**
 *	This function releases the given client space ID.
 *
 *	Note that the space will probably not be immediately deleted - that must
 *	wait until it is no longer referenced (including by chunk dir mappings)
 *
 *	It sets a python error and returns false if the space ID was not a client
 *	space.
 */
static bool releaseSpace( SpaceID spaceID )
{
	ChunkSpacePtr pSpace = getClientSpace( spaceID, "BigWorld.releaseSpace()" );
	// getClientSpace sets the Python error state if it fails
	if (!pSpace)
	{
		return false;
	}


	// find its iterator again (getClientSpace just found it...)
	std::vector<ChunkSpacePtr>::iterator found =
		std::find( s_clientSpaces.begin(), s_clientSpaces.end(), pSpace );
	MF_ASSERT( found != s_clientSpaces.end() );

	// and erase it
	s_clientSpaces.erase( found );
	EntityManager::instance().spaceGone(spaceID);
	return true;
}
PY_AUTO_MODULE_FUNCTION( RETOK, releaseSpace, ARG( SpaceID, END ), BigWorld )


/*~ function BigWorld.addSpaceGeometryMapping
 *
 *	This function maps geometry into the given client space ID.
 *	It cannot be used with spaces created by the server since the server
 *	controls the geometry mappings in those spaces.
 *
 *	Raises a ValueError if the space for the given spaceID is not found.
 *
 *	@param spaceID 		The ID of the space
 *	@param matrix 		The transform to apply to the geometry. None may be
 *						passed in if no transform is required (the identity
 *						matrix).
 *	@param filepath 	The path to the directory containing the space data
 *	@return 			(integer) handle that is used when removing mappings
 *						using BigWorld.delSpaceGeometryMapping().
 *
 */
/**
 *	This function maps geometry into the given client space ID.
 *
 *	It cannot be used with spaces created by the server since the server
 *	controls the geometry mappings in those spaces.
 *
 *	It returns an integer handle which can later be used to unmap it.
 */
static PyObject * addSpaceGeometryMapping( SpaceID spaceID,
	MatrixProviderPtr pMapper, const std::string & path )
{
	ChunkSpacePtr pSpace = getClientSpace( spaceID,
		"BigWorld.addSpaceGeometryMapping()" );
	// getClientSpace sets the Python error state if it fails
	if (!pSpace)
	{
		return false;
	}


	std::string emptyString;

	static uint32 nextLowEntryID = 0;
	uint32 lowEntryID;

	// build the entry id
	SpaceEntryID seid;
	seid.ip = 0;
	do
	{
		lowEntryID = nextLowEntryID++;
		seid.port = uint16(lowEntryID >> 16);
		seid.salt = uint16(lowEntryID);
	}
	// make sure we haven't already used this id,
	// incredibly small chance 'tho it be
	while (pSpace->dataEntry( seid, 1, emptyString ) == uint16(-1));

	// get the matrix
	Matrix m = Matrix::identity;
	if (pMapper) pMapper->matrix( m );

	// see if we can add the mapping
	if (!pSpace->addMapping( seid, m, path ))
	{
		// no good, so remove the data entry
		pSpace->dataEntry( seid, uint16(-1), emptyString );

		PyErr_Format( PyExc_ValueError, "BigWorld.addSpaceGeometryMapping(): "
			"Could not map %s into space ID %d (probably no space.settings)",
			path.c_str(), spaceID );
		return NULL;
	}

	// everything's hunky dory, so return the (low dword of) the entry ID
	return Script::getData( lowEntryID );
}
PY_AUTO_MODULE_FUNCTION( RETOWN, addSpaceGeometryMapping, ARG( SpaceID,
	ARG( MatrixProviderPtr, ARG( std::string, END ) ) ), BigWorld )


/*~ function BigWorld.delSpaceGeometryMapping
 *
 *	This function unmaps geometry from the given client space ID.
 *
 *	It cannot be used with spaces created by the server since the server
 *	controls the geometry mappings in those spaces.
 *
 *	Raises a ValueError if the space for the given spaceID is not found, or if
 *	the handle does not refer to a mapped space geometry.
 *
 *	@param spaceID 	The ID of the space
 *	@param handle 	An integer handle to the space that was returned when
 *					created
 */
/**
 *	This function unmaps geometry from the given client space ID.
 *
 *	It cannot be used with spaces created by the server since the server
 *	controls the geometry mappings in those spaces.
 */
static bool delSpaceGeometryMapping( SpaceID spaceID, uint32 lowEntryID )
{
	ChunkSpacePtr pSpace = getClientSpace( spaceID,
		"BigWorld.delSpaceGeometryMapping()" );
	// getClientSpace sets the Python error state if it fails
	if (!pSpace)
	{
		return false;
	}

	std::string emptyString;

	SpaceEntryID seid;
	seid.ip = 0;
	seid.port = uint16(lowEntryID >> 16);
	seid.salt = uint16(lowEntryID);

	uint16 okey = pSpace->dataEntry( seid, uint16(-1), emptyString );
	if (okey == uint16(-1))
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.delSpaceGeometryMapping(): "
			"Could not unmap entry id %d from space ID %d (no such entry)",
			int(lowEntryID), spaceID );
		return false;
	}

	MF_ASSERT( okey == 1 );	// or else we removed the wrong kind of key!
	pSpace->delMapping( seid );
	return true;
}
PY_AUTO_MODULE_FUNCTION( RETOK, delSpaceGeometryMapping,
	ARG( SpaceID, ARG( uint32, END ) ), BigWorld )


/*~ function BigWorld.clearSpace
 *
 *	This function clears out the given client space ID, including unmapping
 *	all geometry in it. Naturally there should be no entities inside the
 *	space when this is called or else they will be stranded without chunks.
 *	It cannot be used with spaces created by the server since the server
 *	controls the geometry mappings in those spaces.
 *
 *	Raises a ValueError if the space for the given spaceID is not found.
 *
 *	@param spaceID The ID of the space
 */
/**
 *	This function clears out the given client space ID, including unmapping
 *	all geometry in it. Naturally there should be no entities inside the
 *	space when this is called or else they will be stranded without chunks.
 *
 *	It cannot be used with spaces created by the server since the server
 *	controls the geometry mappings in those spaces.
 */
static bool clearSpace( SpaceID spaceID )
{
	ChunkSpacePtr pSpace = getClientSpace( spaceID, "BigWorld.clearSpace()" );
	// getClientSpace sets the Python error state if it fails
	if (!pSpace)
	{
		return false;
	}

	pSpace->clear();
	return true;
}
PY_AUTO_MODULE_FUNCTION( RETOK, clearSpace, ARG( SpaceID, END ), BigWorld )


/*~ function BigWorld.spaceLoadStatus
 *
 *	This function queries the chunk loader to see how much of the current camera
 *	space has been loaded.  It queries the chunk loader to see the distance
 *  to the currently loading chunk ( the chunk loader loads the closest chunks
 *	first ).  A percentage is returned so that scripts can use the information
 *	to create for example a teleportation progress bar.
 *
 *	@param (optional) distance The distance to check for.  By default this is
 *	set to the current far plane.
 *	@return float Rough percentage of the loading status
 */
/**
 *	This function queries the chunk loader to see how much of the current camera
 *	space has been loaded.  It queries the chunk loader to see the distance
 *  to the currently loading chunk ( the chunk loader loads the closest chunks
 *	first. )  A percentage is returned so that scripts can use the information
 *	to create for example a teleportation progress bar.
 */
static float spaceLoadStatus( float distance = -1.f )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();

	// If successful, get closest unloaded chunk distance
	if ( pSpace )
	{
		if (distance < 0.f)
		{
			distance = Moo::rc().camera().farPlane();
		}
		float dist = ChunkManager::instance().closestUnloadedChunk( pSpace );

		return std::min( 1.f, dist / distance );
	}
	else
	{
		// no space, return 0
		return 0.0f;
	}
}
PY_AUTO_MODULE_FUNCTION( RETDATA, spaceLoadStatus, OPTARG( float, -1.f, END ), BigWorld )


/*~ function BigWorld.reloadTextures
 *
 *	This function recompresses all textures to their .dds equivalents,
 *	without restarting the game.  The procedures may take a while, depending
 *	on how many textures are currently loaded by the engine.
 *	This is especially useful if you have updated the texture_detail_levels.xml
 *	file, for example to change normal maps from 32 bit to 16 bits per pixel.
 */
/**
 *	This function recompresses all textures to their .dds equivalents,
 *	without restarting the game.  The procedures may take a while, depending
 *	on how many textures are currently loaded by the engine.
 *	This is especially useful if you have updated the texture_detail_levels.xml
 *	file, for example to change normal maps from 32 bit to 16 bits per pixel.
 */
static void reloadTextures()
{
	Moo::TextureManager::instance()->recalculateDDSFiles();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, reloadTextures, END, BigWorld )


/*~ function BigWorld.restartGame
 *
 *  This function restarts the game.  It can be used to restart the game
 *	after certain graphics options have been changed that require a restart.
 */
/**
 *  This function restarts the game.  It can be used to restart the game
 *	after certain graphics options have been changed that require a restart.
 */
static void restartGame()
{
	App::instance().quit(true);
}
PY_AUTO_MODULE_FUNCTION( RETVOID, restartGame, END, BigWorld )


/*~ function BigWorld.listVideoModes
 *
 *	Lists video modes available on current display device.
 *	@return	list of 5-tuples (int, int, int, int, string).
 *				(mode index, width, height, BPP, description)
 */
static PyObject * listVideoModes()
{
	Moo::DeviceInfo info = Moo::rc().deviceInfo( Moo::rc().deviceIndex() );
	PyObject * result = PyList_New(info.displayModes_.size());

	typedef std::vector< D3DDISPLAYMODE >::const_iterator iterator;
	iterator modeIt = info.displayModes_.begin();
	iterator modeEnd = info.displayModes_.end();
	for (int i=0; modeIt < modeEnd; ++i, ++modeIt)
	{
		PyObject * entry = PyTuple_New(5);
		PyTuple_SetItem(entry, 0, Script::getData(i));
		PyTuple_SetItem(entry, 1, Script::getData(modeIt->Width));
		PyTuple_SetItem(entry, 2, Script::getData(modeIt->Height));

		int bpp = 0;
		switch (modeIt->Format)
		{
		case D3DFMT_A2R10G10B10:
			bpp = 32;
			break;
		case D3DFMT_A8R8G8B8:
			bpp = 32;
			break;
		case D3DFMT_X8R8G8B8:
			bpp = 32;
			break;
		case D3DFMT_A1R5G5B5:
			bpp = 16;
			break;
		case D3DFMT_X1R5G5B5:
			bpp = 16;
			break;
		case D3DFMT_R5G6B5:
			bpp = 16;
			break;
		default:
			bpp = 0;
		}
		PyTuple_SetItem(entry, 3, Script::getData(bpp));
		PyObject * desc = PyString_FromFormat( "%dx%dx%d",
			modeIt->Width, modeIt->Height, bpp);
		PyTuple_SetItem(entry, 4, desc);
		PyList_SetItem(result, i, entry);
	}
	return result;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, listVideoModes, END, BigWorld )


/*~ function BigWorld.changeVideoMode
 *
 *  Changes video mode.
 *	@param	new			int - video mode to use.
 *	@param	windowed	bool - True windowed mode is desired.
 *	@return				bool - True on success. False otherwise.
 */
static bool changeVideoMode( int modeIndex, bool windowed )
{
	return (
		Moo::rc().device() != NULL &&
		Moo::rc().changeMode( modeIndex, windowed ) );
}
PY_AUTO_MODULE_FUNCTION( RETDATA, changeVideoMode, ARG( int, ARG( bool, END ) ), BigWorld )


/*~ function BigWorld.isVideoVSync
 *
 *  Returns current display's vertical sync status.
 *	@return		bool - True if vertical sync is on. False if it is off.
 */
static bool isVideoVSync()
{
	return Moo::rc().device() != NULL
		? Moo::rc().waitForVBL()
		: false;
}
PY_AUTO_MODULE_FUNCTION( RETDATA, isVideoVSync, END, BigWorld )


/*~ function BigWorld.setVideoVSync
 *
 *  Turns vertical sync on/off.
 *	@param	doVSync		bool - True to turn vertical sync on.
 *						False to turn if off.
 */
static void setVideoVSync( bool doVSync )
{
	if (Moo::rc().device() != NULL && doVSync != Moo::rc().waitForVBL())
	{
		Moo::rc().waitForVBL(doVSync);
		Moo::rc().changeMode(Moo::rc().modeIndex(), Moo::rc().windowed());
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, setVideoVSync, ARG( bool, END ), BigWorld )


/*~ function BigWorld.videoModeIndex
 *
 *  Retrieves index of current video mode.
 *	@return	int		Index of current video mode or zero if render
 *					context has not yet been initialized.
 */
static int videoModeIndex()
{
	return
		Moo::rc().device() != NULL
			? Moo::rc().modeIndex()
			: 0;
}
PY_AUTO_MODULE_FUNCTION( RETDATA, videoModeIndex, END, BigWorld )


/*~ function BigWorld.isVideoWindowed
 *
 *  Queries current video windowed state.
 *	@return	bool	True is video is windowed. False if fullscreen.
 */
static bool isVideoWindowed()
{
	return
		Moo::rc().device() != NULL
		? Moo::rc().windowed()
		: false;
}
PY_AUTO_MODULE_FUNCTION( RETDATA, isVideoWindowed, END, BigWorld )


/*~ function BigWorld.resizeWindow
 *
 *  Sets the size of the application window when running in
 *	windowed mode. Does nothing when running in fullscreen mode.
 *
 *	@param width	The desired width of the window.
 *	@param height	The desired width of the window.
 */
static void resizeWindow( int width, int height )
{
	if (Moo::rc().windowed())
	{
		App::instance().resizeWindow(width, height);
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, resizeWindow, ARG( int, ARG( int, END ) ),  BigWorld )


/*~ function BigWorld.windowSize
 *
 *  Returns size of application window when running in windowed mode. This
 *	is different to the screen resolution when running in fullscreen mode
 *	(use listVideoModes and videoModeIndex functions to get the screen
 *	resolution in fullscreen mode).
 *
 *	@return		2-tuple of ints (width, height)
 */
static PyObject * py_windowSize( PyObject * args )
{
	POINT size = App::instance().windowSize();
	PyObject * result = PyTuple_New(2);
	PyTuple_SetItem(result, 0, Script::getData(size.x));
	PyTuple_SetItem(result, 1, Script::getData(size.y));
	return result;
}
PY_MODULE_FUNCTION( windowSize, BigWorld )


/*~ function BigWorld.changeFullScreenAspectRatio
 *
 *  Changes screen aspect ratio for full screen mode.
 *	@param	ratio		the desired aspect ratio: float (width/height).
 */
static void changeFullScreenAspectRatio( float ratio )
{
	if (Moo::rc().device() != NULL)
	{
		Moo::rc().fullScreenAspectRatio( ratio );
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, changeFullScreenAspectRatio, ARG( float, END ), BigWorld )

// -----------------------------------------------------------------------------
// Section: BigWorld module: Basic script services (callbacks and consoles)
// -----------------------------------------------------------------------------

extern double getGameTotalTime();
extern double getServerTime();

/*~ function BigWorld.time
 *
 *	This function returns the client time.  This is the time that the player's
 *	entity is currently ticking at.  Other entities are further back in time,
 *	the further they are from the player entity.
 *
 *	@return		a float.  The time on the client.
 */
/**
 *	Returns the current time.
 */
static PyObject * py_time( PyObject * args )
{
	return PyFloat_FromDouble( getGameTotalTime() );
}
PY_MODULE_FUNCTION( time, BigWorld )


static PyObject * py_serverTime( PyObject * args );
/*~ function BigWorld.stime
 *	@components{ client }
 *	This function is deprecated. BigWorld.serverTime() used be used instead.
 */
/**
 *	Returns the current server time. Retained for backwards-compatibility.
 */
static PyObject * py_stime( PyObject * args )
{
	return py_serverTime( args );
}
PY_MODULE_FUNCTION( stime, BigWorld )


/*~ function BigWorld.serverTime
 *
 *	This function returns the server time.  This is the time that all entities
 *	are at, as far as the server itself is concerned.  This is different from
 *	the value returned by the BigWorld.time() function, which is the time on
 *	the client.
 *
 *	@return		a float.  This is the current time on the server.
 */
/**
 *	Returns the current server time.
 */
static PyObject * py_serverTime( PyObject * args )
{
	return PyFloat_FromDouble( getServerTime() );
}
PY_MODULE_FUNCTION( serverTime, BigWorld )



/// Yes, these are globals
/**
 *	This union is used by the BigWorld client callback
 *	system. It provides a handle to a callback request.
 */
union TimerHandle
{
	uint32 i32;
	struct
	{
		uint16 id;
		uint16 issueCount;
	};
};

typedef std::stack<TimerHandle>		TimerHandleStack;
TimerHandleStack	gFreeTimerHandles;

/**
 *	This structure is used by the BigWorld client callback
 *	system.  It records a single callback request.
 */
struct TimerRecord
{
	/**
	 *	This method returns whether or not the input record occurred later than
	 *	this one.
	 *
	 *	@return True if input record is earlier (higher priority),
	 *		false otherwise.
	 */
	bool operator <( const TimerRecord & b ) const
	{
		return b.time < this->time;
	}

	double		time;			///< The time of the record.
	PyObject	* function;		///< The function associated with the record.
	PyObject	* arguments;	///< The arguments associated with the record.
	const char	* source;		///< The source of this timer record
	TimerHandle	handle;			///< The handle issued for this callback.
};


typedef std::vector<TimerRecord>	Timers;
Timers	gTimers;



/*~ function BigWorld.callback
 *  Registers a callback function to be called after a certain time,
 *  but not before the next tick.
 *  @param time A float describing the delay in seconds before function is
 *  called.
 *  @param function Function to call. This function must take 0 arguments.
 *  @return int A handle that can be used to cancel the callback.
 */
 /**
 *	Registers a callback function to be called after a certain time,
 *	 but not before the next tick. (If registered during a tick
 *	 and it has expired then it will go off still - add a miniscule
 *	 amount of time to BigWorld.time() to prevent this if unwanted)
 *	Non-positive times are interpreted as offsets from the current time.
 */
static PyObject * py_callback( PyObject * args )
{
	double		time = 0.0;
	PyObject *	function = NULL;

	if (!PyArg_ParseTuple( args, "dO", &time, &function ) ||
		function == NULL || !PyCallable_Check( function ) )
	{
		PyErr_SetString( PyExc_TypeError, "py_callback: Argument parsing error." );
		return NULL;
	}

	if (time < 0) time = 0.0;

	time = getGameTotalTime() + time;
	Py_INCREF( function );


	TimerHandle handle;
	if(!gFreeTimerHandles.empty())
	{
		handle = gFreeTimerHandles.top();
		handle.issueCount++;
		gFreeTimerHandles.pop();
	}
	else
	{
		if (gTimers.size() >= USHRT_MAX)
		{
			PyErr_SetString( PyExc_TypeError, "py_callback: Callback handle overflow." );
			return NULL;
		}

		handle.id = gTimers.size() + 1;
		handle.issueCount = 1;
	}


	TimerRecord		newTR =
	{ time, function, PyTuple_New(0), "BigWorld Callback: ", { handle.i32 } };
	gTimers.push_back( newTR );


	PyObject * pyId = PyInt_FromLong(handle.i32);
	return pyId;
}
PY_MODULE_FUNCTION( callback, BigWorld )


/*~ function BigWorld.cancelCallback
 *  Cancels a previously registered callback.
 *  @param int An integer handle identifying the callback to cancel.
 *  @return None.
 */
 /**
 *	Cancels a previously registered callback.
 *   Safe behavior is NOT guarantied when canceling an already executed
 *   or canceled callback.
 */
static PyObject * py_cancelCallback( PyObject * args )
{
	TimerHandle handle;


	if (!PyArg_ParseTuple( args, "i", &handle.i32 ) )
	{
		PyErr_SetString( PyExc_TypeError, "py_cancelCallback: Argument parsing error." );
		return NULL;
	}


	for( Timers::iterator iTimer = gTimers.begin(); iTimer != gTimers.end(); iTimer++ )
	{
		if( iTimer->handle.i32 == handle.i32 )
		{
			Py_DECREF( iTimer->function );
			Py_DECREF( iTimer->arguments );
			gFreeTimerHandles.push( iTimer->handle );
			std::swap<>( *iTimer, gTimers.back() );
			gTimers.pop_back();
			break;
		}
	}

	Py_Return;
}
PY_MODULE_FUNCTION( cancelCallback, BigWorld )






static XConsole * s_pOutConsole = NULL;
static XConsole * s_pErrConsole = NULL;

/**
 *	This class implements a PyOutputWriter with the added functionality of
 *	writing to the Python console.
 */
class BWOutputWriter : public PyOutputWriter
{
public:
	BWOutputWriter( bool errNotOut, const char * fileText ) :
		PyOutputWriter( fileText, /*shouldWritePythonLog = */true ),
		errNotOut_( errNotOut )
	{
		 // TODO: Should make writing Python log configuarable.
	}

private:
	virtual void printMessage( const char * msg );

	bool	errNotOut_;
};


/**
 *	This method implements the default behaviour for printing a message. Derived
 *	class should override this to change the behaviour.
 */
void BWOutputWriter::printMessage( const char * msg )
{
	RemoteStepper::step( msg, false );

	XConsole * pXC = errNotOut_ ? s_pErrConsole : s_pOutConsole;
	if (pXC != NULL) pXC->print( msg );

	this->PyOutputWriter::printMessage( msg );
}







// -----------------------------------------------------------------------------
// Section: BigWorldClientScript namespace functions
// -----------------------------------------------------------------------------

/// Reference the modules we want to use, to make sure they are linked in.
extern int BoundingBoxGUIComponent_token;
extern int PySceneRenderer_token;
static int GUI_extensions_token =
	BoundingBoxGUIComponent_token |
	PySceneRenderer_token;

extern int FlexiCam_token;
extern int CursorCamera_token;
extern int FreeCamera_token;
static int Camera_token =
	FlexiCam_token |
	CursorCamera_token |
	FreeCamera_token;

extern int PyChunkModel_token;
static int Chunk_token = PyChunkModel_token;

extern int Math_token;
extern int GUI_token;
extern int ResMgr_token;
extern int Pot_token;
extern int PyScript_token;
static int s_moduleTokens =
	Math_token |
	GUI_token |
	GUI_extensions_token |
	ResMgr_token |
	Camera_token |
	Chunk_token |
	Pot_token |
	PyScript_token;


//extern void memNow( const std::string& token );

/**
 *	Client scripting initialisation function
 */
bool BigWorldClientScript::init()
{
//	memNow( "Before base script init" );

	RemoteStepper::step( "Script::init" );
	// Call the general init function
	if (!Script::init( "entities/client", "client" ) )
	{
		return false;
	}

//	memNow( "After base script init" );

	RemoteStepper::step( "Import BigWorld" );

	// Initialise the BigWorld module
	PyObject * pBWModule = PyImport_AddModule( "BigWorld" );

	// Set the 'Entity' class into it as an attribute
	if (PyObject_SetAttrString( pBWModule, "Entity",
			(PyObject *)&Entity::s_type_ ) == -1)
	{
		return false;
	}

	// We implement our own stderr / stdout so we can see Python's output
	PyObject * pSysModule = PyImport_AddModule( "sys" );	// borrowed

	char fileText[ 256 ];

#ifdef _DEBUG
		const char * config = "Debug";
#elif defined( _HYBRID )
		const char * config = "Hybrid";
#else
		const char * config = "Release";
#endif

	time_t aboutTime = time( NULL );

	sprintf( fileText, "BigWorld %s Client (compiled at %s) starting on %s",
		config,
		compileTimeString,
		ctime( &aboutTime ) );

#if !BWCLIENT_AS_PYTHON_MODULE
	RemoteStepper::step( "Overridding stdout/err" );

	PyObject_SetAttrString( pSysModule,
		"stderr", new BWOutputWriter( true, fileText ) );

	PyObject_SetAttrString( pSysModule,
		"stdout", new BWOutputWriter( false, fileText ) );

	// TODO: Should decrement these new objects.

	RemoteStepper::step( "EntityType init" );
#endif // !BWCLIENT_AS_PYTHON_MODULE

	// Load all the standard entity scripts
	bool ret = EntityType::init();

	RemoteStepper::step( "BigWorldClientScript finished" );

	return ret;
}


/**
 *	Client scripting termination function
 */
void BigWorldClientScript::fini()
{
	PyOutputWriter::fini();
	Script::fini();
}

void BigWorldClientScript::clearSpaces()
{
	// this has to be called at a different time
	// than fini, that's why it's a separate method
	while (!s_clientSpaces.empty())
	{
		s_clientSpaces.back()->clear();
		s_clientSpaces.pop_back();
	}
}


void BigWorldClientScript::clearTimers()
{
	// this has to be called at a different time
	// than fini, that's why it's a separate method
	for( Timers::iterator iTimer = gTimers.begin(); iTimer != gTimers.end(); iTimer++ )
	{
		Py_DECREF( iTimer->function );
		Py_DECREF( iTimer->arguments );
	}
	gTimers.clear();
}


/**
 *	This function calls any script timers which have expired by now
 */
void BigWorldClientScript::tick( double timeNow )
{
	int numExpired = 0;
	const int MAX_TIMER_CALLS_PER_FRAME = 1000;


	for ( uint iTimer=0; iTimer < gTimers.size() && numExpired < MAX_TIMER_CALLS_PER_FRAME; iTimer++)
	{
		if ( gTimers[iTimer].time <= timeNow )
		{
			TimerRecord timer = gTimers[iTimer];
			gTimers[iTimer] = gTimers.back();
			gTimers.pop_back();

			gFreeTimerHandles.push( timer.handle );

			Script::call( timer.function, timer.arguments, timer.source );
			// Script::call decrefs timer.function and timer.arguments for us

			numExpired++;
		}
	}

	if (numExpired >= MAX_TIMER_CALLS_PER_FRAME)
	{
		ERROR_MSG( "BigWorldClientScript::tick: Loop interrupted because"
			" too many timers (> %d) wanted to expire this frame!",
			numExpired );
	}
}


/**
 *	This function adds a script 'timer' to be called next tick
 *
 *	It is used by routines which want to make script calls but can't
 *	because they're in the middle of something scripts might mess up
 *	(like iterating over the scene to tick or draw it)
 *
 *	The optional age parameter specifies the age of the call,
 *	i.e. how far in the past it wanted to be made.
 *	Older calls are called back first.
 *
 *	@note: This function steals the references to both fn and args
 */
void BigWorldClientScript::callNextFrame( PyObject * fn, PyObject * args,
	const char * reason, double age )
{
	TimerHandle handle;
	if(!gFreeTimerHandles.empty())
	{
		handle = gFreeTimerHandles.top();
		handle.issueCount++;
		gFreeTimerHandles.pop();
	}
	else
	{
		if (gTimers.size() >= USHRT_MAX)
		{
			PyErr_SetString( PyExc_TypeError, "callNextFrame: Callback handle overflow." );
			return;
		}
		handle.id = gTimers.size() + 1;
		handle.issueCount = 1;
	}

	TimerRecord		newTR = { -age, fn, args, reason, { handle.i32 } };
	gTimers.push_back( newTR );
}



/**
 *	This function gets the consoles that python output and errors appear on.
 */
void BigWorldClientScript::getPythonConsoles(
	XConsole *& pOutConsole, XConsole *& pErrConsole )
{
	pOutConsole = s_pOutConsole;
	pErrConsole = s_pErrConsole;
}

/**
 *	This function sets the consoles that python output and errors appear on.
 */
void BigWorldClientScript::setPythonConsoles(
	XConsole * pOutConsole, XConsole * pErrConsole )
{
	s_pOutConsole = pOutConsole;
	s_pErrConsole = pErrConsole;
}


// TODO:PM This is probably not the best place for this.
/**
 *	This function adds an alert message to the display.
 */
bool BigWorldClientScript::addAlert( const char * alertType, const char * alertName )
{
	bool succeeded = false;

	PyObjectPtr pModule = PyObjectPtr(
			PyImport_ImportModule( "Helpers.alertsGui" ),
			PyObjectPtr::STEAL_REFERENCE );

	if (pModule)
	{
		PyObjectPtr pInstance = PyObjectPtr(
			PyObject_GetAttrString( pModule.getObject(), "instance" ),
			PyObjectPtr::STEAL_REFERENCE );

		if (pInstance)
		{
			PyObjectPtr pResult = PyObjectPtr(
				PyObject_CallMethod( pInstance.getObject(),
									"add", "ss", alertType, alertName ),
				PyObjectPtr::STEAL_REFERENCE );

			if (pResult)
			{
				succeeded = true;
			}
		}
	}

	if (!succeeded)
	{
		PyErr_PrintEx(0);
		WARNING_MSG( "BigWorldClientScript::addAlert: Call failed.\n" );
	}

	return succeeded;
}

class MyFunkySequence : public PySTLSequenceHolder< std::vector<std::string> >
{
public:
	MyFunkySequence() :
		PySTLSequenceHolder< std::vector<std::string> >( strings_, NULL, true )
	{}

	std::vector<std::string>	strings_;
};

/*~ function BigWorld createTranslationOverrideAnim
 *  This function is a tool which can be used to alter skeletal animations so
 *  that they can be used with models which have skeletons of different
 *  proportions. This is achieved by creating a new animation which is based
 *  on a given animation, but replaces the translation component for each node
 *  with that of the beginning of the same node in a reference animation. As
 *  the translation should not change in a skeletal system (bones do not change
 *  size or shape), this effectively re-fits the animation on to a differently
 *  proportioned model. This operates by creating a new animation file, and
 *  is not intended for in-game use.
 *  @param baseAnim A string containing the name (including path) of the
 *  animation file on which the new file is to be based.
 *  @param translationReferenceAnim A string containing the name (including path)
 *  of the animation file whose first frame contains the translation which will
 *  be used for the new animation. This should have the same proportions as are
 *  desired for the new animation.
 *  @param noOverrideChannels A list of strings containing the names of the nodes
 *  that shouldn't have their translation overridden. These nodes will not be
 *  scaled to the proportions provided by translationReferenceAnim in the new
 *  animation.
 *  @param outputAnim A string containing the name (including path) of the
 *  animation file to which the new animation will be saved.
 *  @return None
 */
static void createTranslationOverrideAnim( const std::string& baseAnim,
										  const std::string& translationReferenceAnim,
										  const MyFunkySequence& noOverrideChannels,
										  const std::string& outputAnim )
{
	Moo::AnimationPtr pBase = Moo::AnimationManager::instance().find( baseAnim );
	if (!pBase.hasObject())
	{
		CRITICAL_MSG( "createTranslationOverrideAnim - Unable to open animation %s\n", baseAnim.c_str() );
		return;
	}

	Moo::AnimationPtr pTransRef = Moo::AnimationManager::instance().find( translationReferenceAnim );
	if (!pTransRef.hasObject())
	{
		CRITICAL_MSG( "createTranslationOverrideAnim - Unable to open animation %s\n", translationReferenceAnim.c_str() );
		return;
	}
	Moo::AnimationPtr pNew = new Moo::Animation();

	pNew->translationOverrideAnim( pBase, pTransRef, noOverrideChannels.strings_ );

	pNew->save( outputAnim );
}

PY_AUTO_MODULE_FUNCTION( RETVOID, createTranslationOverrideAnim, ARG(
	std::string, ARG( std::string, ARG( MyFunkySequence, ARG( std::string,
	END ) ) ) ), BigWorld )

/*~ function BigWorld.memUsed
 *	@components{ client }
 *
 *	This function returns an estimate of the amount of memory the application
 *	is currently using.
 */
extern uint32 memUsed();
PY_AUTO_MODULE_FUNCTION( RETDATA, memUsed, END, BigWorld )

/*~ function BigWorld.screenWidth
 *	Returns the width of the current game window.
 *	@return float
 */
float screenWidth()
{
	return Moo::rc().screenWidth();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, screenWidth, END, BigWorld )

/*~ function BigWorld.screenHeight
 *	Returns the height of the current game window.
 *	@return float
 */
float screenHeight()
{
	return Moo::rc().screenHeight();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, screenHeight, END, BigWorld )

/*~ function BigWorld.screenSize
 *	Returns the width and height of the current game window as a tuple.
 *	@return (float, float)
 */
PyObject * screenSize()
{
	float width = Moo::rc().screenWidth();
	float height = Moo::rc().screenHeight();
	PyObject * pTuple = PyTuple_New( 2 );
	PyTuple_SetItem( pTuple, 0, Script::getData( width ) );
	PyTuple_SetItem( pTuple, 1, Script::getData( height ) );
	return pTuple;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, screenSize, END, BigWorld )


/*~ function BigWorld.screenShot()
 *	This method takes a screenshot and writes the bitmap to your
 *	games' executable folder.
 */
void screenShot()
{
	Moo::rc().screenShot();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, screenShot, END, BigWorld )


/*~ function BigWorld.connectedEntity
 *	This method returns the entity that this application is connected to. The
 *	connected entity is the server entity that is responsible for collecting and
 *	sending data to this client application. It is also the only client entity
 *	that has an Entity.base property.
 */
Entity * connectedEntity()
{
	ObjectID connectedID = 0;
	if (EntityManager::instance().pServer())
	{
		connectedID = EntityManager::instance().pServer()->connectedID();
	}
	return EntityManager::instance().getEntity( connectedID );
}
PY_AUTO_MODULE_FUNCTION( RETDATA, connectedEntity, END, BigWorld )



/*~ function BigWorld.savePreferences
 *
 *  Saves the current preferences (video, graphics and script) into
 *	a XML file. The name of  the file to the writen is defined in the
 *	<preferences> field in engine_config.xml.
 *
 *	@return		bool	True on success. False on error.
 */
static bool savePreferences()
{
	return App::instance().savePreferences();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, savePreferences, END, BigWorld )

#include "resmgr/multi_file_system.hpp"
typedef std::map< PyObject* , std::vector<PyObject*> > ReferersMap;
/*~ function BigWorld.dumpRefs
 *	Dumps references to each object visible from the entity tree.
 *	This does not include all objects, and may not include all references.
 */
void dumpRefs()
{
	// Build a map of the objects that reference each other object.
	ReferersMap	referersMap;

	PyObject * pSeed = PyImport_AddModule( "BigWorld" );
	referersMap[pSeed].push_back( pSeed );	// refers to itself for seeding

	std::vector<PyObject*> stack;
	stack.push_back( pSeed );
	while (!stack.empty())
	{
		PyObject * pLook = stack.back();
		stack.pop_back();

		// go through all objects accessible from here

		// look in the dir and get attributes
		PyObject * pDirSeq = PyObject_Dir( pLook );
		int dlen = 0;
		if (pDirSeq != NULL)
		{
			dlen = PySequence_Length( pDirSeq );
		} else { PyErr_Clear(); }
		for (int i = 0; i < dlen; i++)
		{
			PyObject * pRefereeName = PySequence_GetItem( pDirSeq, i );
			PyObject * pReferee = PyObject_GetAttr( pLook, pRefereeName );
			Py_DECREF( pRefereeName );
			if (pReferee == NULL)
			{
				// shouldn't get errors like this (hmmm)
				PyErr_Clear();
				WARNING_MSG( "%s in dir of 0x%08X but cannot access it\n",
					PyString_AsString( pRefereeName ), pLook );
				continue;
			}
			if (pReferee->ob_refcnt == 1)
			{
				// if it was created just for us we don't care
				Py_DECREF( pReferee );
				continue;
			}
			// ok we have an object that is part of the tree in pReferee

			// find/create the vector of referers to pReferee
			std::vector<PyObject*> & referers = referersMap[pReferee];
			// if pLook is first to refer to this obj then traverse it (later)
			if (referers.empty())
				stack.push_back( pReferee );
			// record the fact that pLook refers to this obj
			referers.push_back( pLook );

			Py_DECREF( pReferee );
		}
		Py_XDECREF( pDirSeq );

		// look in the sequence
		int slen = 0;
		if (PySequence_Check( pLook )) slen = PySequence_Size( pLook );
		for (int i = 0; i < slen; i++)
		{
			PyObject * pReferee = PySequence_GetItem( pLook, i );
			if (pReferee == NULL)
			{
				// _definitely_ shouldn't get errors like this! (but do)
				PyErr_Clear();
				WARNING_MSG( "%d seq in 0x%08X but cannot access item %d\n",
					slen, pLook, i );
				continue;
			}
			MF_ASSERT( pReferee != NULL );
			if (pReferee->ob_refcnt == 1)
			{
				// if it was created just for us we don't care
				Py_DECREF( pReferee );
				continue;
			}

			// find/create the vector of referers to pReferee
			std::vector<PyObject*> & referers = referersMap[pReferee];
			// if pLook is first to refer to this obj then traverse it (later)
			if (referers.empty())
				stack.push_back( pReferee );
			// record the fact that pLook refers to this obj
			referers.push_back( pLook );

			Py_DECREF( pReferee );
		}

		// look in the mapping
		PyObject * pMapItems = NULL;
		int mlen = 0;
		if (PyMapping_Check( pLook ))
		{
			pMapItems = PyMapping_Items( pLook );
			mlen = PySequence_Size( pMapItems );
		}
		for (int i = 0; i < mlen; i++)
		{
		  PyObject * pTuple = PySequence_GetItem( pMapItems, i );
		  int tlen = PySequence_Size( pTuple );
		  for (int j = 0; j < tlen; j++)
		  {
			PyObject * pReferee = PySequence_GetItem( pTuple, j );
			MF_ASSERT( pReferee != NULL );
			if (pReferee->ob_refcnt == 2)
			{
				// if it was created just for us we don't care
				Py_DECREF( pReferee );
				continue;
			}

			// find/create the vector of referers to pReferee
			std::vector<PyObject*> & referers = referersMap[pReferee];
			// if pLook is first to refer to this obj then traverse it (later)
			if (referers.empty())
				stack.push_back( pReferee );
			// record the fact that pLook refers to this obj
			referers.push_back( pLook );

			Py_DECREF( pReferee );
		  }
		  Py_DECREF( pTuple );
		}
		Py_XDECREF( pMapItems );
	}

	time_t now = time( &now );
	std::string nowStr = ctime( &now );
	nowStr.erase( nowStr.end()-1 );
	FILE * f = BWResource::instance().fileSystem()->posixFileOpen(
		"py ref table.txt", "a" );
	fprintf( f, "\n" );
	fprintf( f, "List of references to all accessible from 'BigWorld':\n" );
	fprintf( f, "(as at %s)\n", nowStr.c_str() );
	fprintf( f, "-----------------------------------------------------\n" );

	// Now print out all the objects and their referers
	ReferersMap::iterator it;
	for (it = referersMap.begin(); it != referersMap.end(); it++)
	{
		PyObject * pReferee = it->first;
		PyObject * pRefereeStr = PyObject_Str( pReferee );
		fprintf( f, "References to object at 0x%08X type %s "
				"aka '%s' (found %d/%d):\n",
			pReferee, pReferee->ob_type->tp_name,
			PyString_AsString( pRefereeStr ),
			it->second.size(), pReferee->ob_refcnt );
		Py_DECREF( pRefereeStr );

		for (uint i = 0; i < it->second.size(); i++)
			fprintf( f, "\t0x%08X\n", it->second[i] );
		fprintf( f, "\n" );
	}

	fprintf( f, "-----------------------------------------------------\n" );
	fprintf( f, "\n" );
	fclose( f );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, dumpRefs, END, BigWorld )

// script_bigworld.cpp
