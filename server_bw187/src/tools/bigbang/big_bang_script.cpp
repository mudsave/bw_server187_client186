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
#include "big_bang_script.hpp"
#include "big_bang_camera.hpp"
#include "snaps.hpp"

#include "appmgr/commentary.hpp"
#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"

#include "cstdmf/debug.hpp"
#include "compile_time.hpp"

#include "input/input.hpp"

#include "pyscript/res_mgr_script.hpp"
#include "pyscript/script.hpp"
#include "pyscript/script_math.hpp"
#include "pyscript/py_output_writer.hpp"

#include "romp/fog_controller.hpp"
#include "romp/weather.hpp"
#include "romp/console_manager.hpp"
#include "romp/xconsole.hpp"

#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "terrain/terrain_locator.hpp"

#include "xactsnd/soundmgr.hpp"

#include "big_bang.hpp"
#include "gizmo/tool_manager.hpp"
#include "../common/base_camera.hpp"
#include "gizmo/undoredo.hpp"

#include "panel_manager.hpp"

#include <queue>


#ifndef CODE_INLINE
#include "big_bang_script.ipp"
#endif


DECLARE_DEBUG_COMPONENT2( "Script", 0 )

// -----------------------------------------------------------------------------
// Section: 'BigBang' Module
// -----------------------------------------------------------------------------


/**
 *	This function implements a script function. It returns whether or not the
 *	given key is down.
 */
static PyObject * py_isKeyDown( PyObject * args )
{
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_isKeyDown: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( InputDevices::isKeyDown( (KeyEvent::Key)key ) );
}
PY_MODULE_FUNCTION( isKeyDown, BigBang )


/**
 *	This function implements a script function. It returns whether or not
 *	CapsLock is on.
 */
static PyObject * py_isCapsLockOn( PyObject * args )
{
	return PyInt_FromLong(
		(::GetKeyState( VK_CAPITAL ) & 0x0001) == 0 ? 0 : 1 );
}
PY_MODULE_FUNCTION( isCapsLockOn, BigBang )


/**
 *	This function implements a script function. It returns the key given its
 *	name
 */
static PyObject * py_stringToKey( PyObject * args )
{
	char * str;
	if (!PyArg_ParseTuple( args, "s", &str ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_stringToKey: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( KeyEvent::stringToKey( str ) );
}
PY_MODULE_FUNCTION( stringToKey, BigBang )


/**
 *	This function implements a script function. It returns the name of the given
 *	key.
 */
static PyObject * py_keyToString( PyObject * args )
{
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_keyToString: Argument parsing error." );
		return NULL;
	}

	return PyString_FromString( KeyEvent::keyToString(
		(KeyEvent::Key) key ) );
}
PY_MODULE_FUNCTION( keyToString, BigBang )


/**
 *	This function implements a script function. It returns the value of the
 *	given joypad axis.
 */
static PyObject * py_axisValue( PyObject * args )
{
	int	axis;
	if (!PyArg_ParseTuple( args, "i", &axis ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_axisValue: Argument parsing error." );
		return NULL;
	}

	Joystick::Axis a = InputDevices::joystick().getAxis( (AxisEvent::Axis)axis );

	return PyFloat_FromDouble( a.value() );
}

PY_MODULE_FUNCTION( axisValue, BigBang )


/**
 *	This function implements a script function. It returns the direction of the
 *	given joypad axis.
 */
static PyObject * py_axisDirection( PyObject * args )
{
	int	axis;
	if (!PyArg_ParseTuple( args, "i", &axis ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_axisPosition: Argument parsing error." );
		return NULL;
	}

	int direction = InputDevices::joystick().stickDirection( (AxisEvent::Axis)axis );

	return PyInt_FromLong( direction );
}
PY_MODULE_FUNCTION( axisDirection, BigBang )


/**
 *	This function implements a script function. It plays the named sound effect.
 */
static PyObject* py_playFx( PyObject * args )
{
	char* tag;
	float x, y, z;

	if (!PyArg_ParseTuple( args, "s(fff)", &tag , &x, &y, &z ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playFx: Argument parsing error." );
		return NULL;
	}

	//TRACE_MSG( "py_playFx(%s)\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
	//soundMgr().playFx(tag, Vector3(x, y, z));

	Py_Return;
}
PY_MODULE_FUNCTION( playFx, BigBang )


/**
 *	This function implements a script function. It plays the named sound effect
 *	with a delay.
 */
static PyObject * py_playFxDelayed( PyObject * args )
{
	char* tag;
	float x, y, z, delay;

	if (!PyArg_ParseTuple( args, "s(fff)f", &tag , &x, &y, &z, &delay ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playFxDelayed: Argument parsing error." );
		return NULL;
	}

	//TRACE_MSG( "py_playFxDelayed(%s)\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
	//soundMgr().playFxDelayed(tag, delay, Vector3(x, y, z));

	Py_Return;
}
PY_MODULE_FUNCTION( playFxDelayed, BigBang )


/**
 *	This function implements a script function. It returns a reference to a
 *	loaded sound.
 */
static PyObject* py_fxSound( PyObject* args )
{
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag )||1)// unimplement
	{
		PyErr_SetString( PyExc_TypeError, "py_fxSound: Argument parsing error." );
		return NULL;
	}

	DEBUG_MSG( "py_fxSound: %s\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
/*	PyFxSound* snd = new PyFxSound( tag );

	if (!snd->isValidForPy())
	{
		PyErr_Format( PyExc_ValueError, "py_fxSound: No such sound: %s", tag );
		Py_DECREF( snd );
		return NULL;
	}

	return snd;*/
}
PY_MODULE_FUNCTION( fxSound, BigBang )


/**
 *	This function implements a script function. It plays the named Simple sound.
 */
static PyObject * py_playSimple( PyObject * args )
{
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playSimple: Argument parsing error." );
		return NULL;
	}

	TRACE_MSG( "py_playSimple(%s)\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
	//soundMgr().playSimple(tag);

	Py_Return;
}
PY_MODULE_FUNCTION( playSimple, BigBang )



/**
 *	This function implements a script function. It adds a message to the
 *	commentary console.
 */
static PyObject * py_addCommentaryMsg( PyObject * args )
{
	int id = Commentary::COMMENT;
	char* tag;

	if (!PyArg_ParseTuple( args, "s|i", &tag, &id ))
	{
		PyErr_SetString( PyExc_TypeError, "py_addCommentaryMsg: Argument parsing error." );
		return NULL;
	}

	if ( stricmp( tag, "" ) )
	{
		Commentary::instance().addMsg( std::string( tag ), id );
		dprintf( "Commentary: %s\n", tag );
	}
	else
	{
		Commentary::instance().addMsg( std::string( "NULL" ), Commentary::WARNING );
	}

	Py_Return;
}
PY_MODULE_FUNCTION( addCommentaryMsg, BigBang )


/**
 *	This function implements a script function. It pushes a module
 *	onto the application's module stack.
 */
static PyObject * py_push( PyObject * args )
{
	char* id;

	if (!PyArg_ParseTuple( args, "s", &id ))
	{
		PyErr_SetString( PyExc_TypeError, "py_push: Argument parsing error." );
		return NULL;
	}

	ModuleManager::instance().push( std::string(id) );

	Py_Return;
}
PY_MODULE_FUNCTION( push, BigBang )


/**
 *	This function implements a script function. It pops the current
 *	module from the application's module stack.
 */
static PyObject * py_pop( PyObject * args )
{
	ModuleManager::instance().pop();

	Py_Return;
}
PY_MODULE_FUNCTION( pop, BigBang )


/**
 *	This function implements a script function. It pushes a tool
 *	onto bigbang's tool stack.
 */
static PyObject * py_pushTool( PyObject * args )
{
	PyObject* pTool;

	if (!PyArg_ParseTuple( args, "O", &pTool ) ||
		!Tool::Check( pTool ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_pushTool: Expected a Tool." );
		return NULL;
	}

	ToolManager::instance().pushTool( static_cast<Tool*>( pTool ) );

	Py_Return;
}
PY_MODULE_FUNCTION( pushTool, BigBang )


/**
 *	This function implements a script function. It pops the current
 *	tool from bigbang's tool stack.
 */
static PyObject * py_popTool( PyObject * args )
{
	ToolManager::instance().popTool();

	Py_Return;
}
PY_MODULE_FUNCTION( popTool, BigBang )


/**
 *	This function implements a script function. It gets the
 *	current tool from BigBang's tool stack.
 */
static PyObject * py_tool( PyObject * args )
{
	ToolPtr spTool = ToolManager::instance().tool();

	if (spTool)
	{
		Py_INCREF( spTool.getObject() );
		return spTool.getObject();
	}
	else
	{
		Py_Return;
	}
}
PY_MODULE_FUNCTION( tool, BigBang )


/**
 *	This function undoes the most recent operation, returning
 *	its description. If it is passed a positive integer argument,
 *	then it just returns the description for that level of the
 *	undo stack and doesn't actually undo anything.
 *	If there is no undo level, the empty string is returned.
 */
static PyObject * py_undo( PyObject * args )
{
    CWaitCursor waitCursor;

	int forStep = -1;
	if (!PyArg_ParseTuple( args, "|i", &forStep ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.undo() "
			"expects an optional integer argument" );
		return NULL;
	}

	std::string what = UndoRedo::instance().undoInfo( max(0,forStep) );

	if (forStep < 0) UndoRedo::instance().undo();

	return Script::getData( what );
}
PY_MODULE_FUNCTION( undo, BigBang )

/**
 *	This function works exactly like undo, it just redoes.
 *
 *	@see py_undo
 */
static PyObject * py_redo( PyObject * args )
{
    CWaitCursor waitCursor;

	int forStep = -1;
	if (!PyArg_ParseTuple( args, "|i", &forStep ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.redo() "
			"expects an optional integer argument" );
		return NULL;
	}

	std::string what = UndoRedo::instance().redoInfo( max(0,forStep) );

	if (forStep < 0) UndoRedo::instance().redo();

	return Script::getData( what );
}
PY_MODULE_FUNCTION( redo, BigBang )

/**
 * Adds an undo/redo barrier with the given name
 */
static PyObject * py_addUndoBarrier( PyObject * args )
{

	char* name;
	int skipIfNoChange = 0;
	if (!PyArg_ParseTuple( args, "s|i", &name, &skipIfNoChange ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.addUndoBarrier() "
			"expects a string and an optional int" );
		return NULL;
	}

	// Add the undo barrier
	UndoRedo::instance().barrier( name, (skipIfNoChange != 0) );

	Py_Return;
}
PY_MODULE_FUNCTION( addUndoBarrier, BigBang )


/**
 *	This function saves the options file.
 */
static PyObject * py_saveOptions( PyObject * args )
{
	char * filename = NULL;

	if (!PyArg_ParseTuple( args, "|s", &filename ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.saveOptions() "
			"expects an optional string argument." );
		return NULL;
	}

	return Script::getData( Options::save( filename ) );
}
PY_MODULE_FUNCTION( saveOptions, BigBang )


/**
 *	This function gets a BigBang camera
 *	- if no argument specified, the current camera is returned
 *	- else the specified camera is returned
 */
static PyObject * py_camera( PyObject * args )
{
	int cameraType = -1;
	if (!PyArg_ParseTuple( args, "|i", &cameraType ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.camera() "
			"expects an optional int argument." );
		return NULL;
	}

	if (cameraType == -1)
	{
		// if no camera specified, return the current camera
		return Script::getData( &(BigBangCamera::instance().currentCamera()) );
	}
	else
	{
		// else return the camera specified (only one type of each camera exists
		return Script::getData( &(BigBangCamera::instance().camera((BigBangCamera::CameraType)cameraType)) );
	}
}
PY_MODULE_FUNCTION( camera, BigBang )


/**
 *	This function gets a BigBang camera
 *	- if no argument specified, the current camera is returned
 *	- else the specified camera is returned
 */
static PyObject * py_changeToCamera( PyObject * args )
{
	int cameraType = -1;
	if (!PyArg_ParseTuple( args, "i", &cameraType ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.camera() "
			"expects an int argument." );
		return NULL;
	}

	if (cameraType != -1)
		BigBangCamera::instance().changeToCamera((BigBangCamera::CameraType)cameraType);

	Py_Return;
}
PY_MODULE_FUNCTION( changeToCamera, BigBang )


/**
 *	This function is a temporary one to snap the camera to the ground.
 */
static PyObject * py_snapCameraToTerrain( PyObject * args )
{
	BaseCamera& cam = BigBangCamera::instance().currentCamera();

	Matrix view = cam.view();
	view.invert();
	Vector3 camPos( view.applyToOrigin() );

	ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
	if ( space )
	{
		ClosestTerrainObstacle::s_default.dist_ = 0.f;
		// magic numbers are defined here:
		const float EXTENT_RANGE	= 5000.0f;
		const float CAM_RANGE		= 5000.0f;

		// start with the camera's vertical position at 0m
		camPos.y = 0;
		// cycle incrementing the camera's vertical position until a collision is 
		// found, or until the camera's maximum range is reached (set very high!).
		while( ClosestTerrainObstacle::s_default.dist_ == 0.f )
		{
			Vector3 extent = camPos + ( Vector3( 0, -EXTENT_RANGE, 0.f ) );

			space->collide( 
				camPos,
				extent,
				ClosestTerrainObstacle::s_default );

			// clamp the camera max height to something 'sensible'
			if ( camPos.y >= CAM_RANGE )
				break;

			if ( ClosestTerrainObstacle::s_default.dist_ == 0.f )
			{
				// drop the camera from higher if no collision is detected
				camPos.y += 200;
			}
		}

		if ( ClosestTerrainObstacle::s_default.dist_ > 0.f )
		{
			camPos = camPos +
					( Vector3(0,-1,0) * ClosestTerrainObstacle::s_default.dist_ );
			view.translation( camPos +
				Vector3( 0,
				(float)Options::getOptionFloat( "graphics/cameraHeight", 2.f ),
				0 ) );
			view.invert();
			cam.view( view );
		}
	}	

	Py_Return;
}
PY_MODULE_FUNCTION( snapCameraToTerrain, BigBang )

/**
 *	This function tells bigbang we enter playerPreviewMode
 */
static PyObject * py_enterPlayerPreviewMode( PyObject * args )
{
	BigBang::instance().setPlayerPreviewMode( true );
	Py_Return;
}
PY_MODULE_FUNCTION( enterPlayerPreviewMode, BigBang )

/**
 *	This function tells bigbang we leave playerPreviewMode
 */
static PyObject * py_leavePlayerPreviewMode( PyObject * args )
{
	BigBang::instance().setPlayerPreviewMode( false );
	Py_Return;
}
PY_MODULE_FUNCTION( leavePlayerPreviewMode, BigBang )

/**
 *	This function asks bigbang if we are in playerPreviewMode
 */
static PyObject * py_isInPlayerPreviewMode( PyObject * args )
{
	return PyInt_FromLong( BigBang::instance().isInPlayerPreviewMode() );
}
PY_MODULE_FUNCTION( isInPlayerPreviewMode, BigBang )

/**
 *	This is a temporary function that simply makes the camera
 *	go top-down.
 */
static PyObject * py_fudgeOrthographicMode( PyObject * args )
{
	float height = -31000.f;
	float lag = 5.f;

	if ( !PyArg_ParseTuple( args, "|ff", &height, &lag ) )
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.fudgeOrthographicMode() "
			"expects two optional float arguments - height and lag." );
		return NULL;
	}


	BaseCamera& cam = BigBangCamera::instance().currentCamera();

	Matrix view = cam.view();
	view.invert();
	Vector3 camPos( view.applyToOrigin() );

	if ( height > -30000.f )
	{
		if ( camPos.y != height )
		{
			float newCamY = ( ( (camPos.y*lag) + height ) / (lag+1.f) );
			float dy = ( newCamY - camPos.y ) * BigBang::instance().dTime();
			camPos.y += dy;
		}
	}

	cam.view().setRotateX( 1.570796326794f );
	cam.view().postTranslateBy( camPos );
	cam.view().invert();

	Py_Return;
}
PY_MODULE_FUNCTION( fudgeOrthographicMode, BigBang )



/**
 *	This test function ejects the chunk under the current tool's locator
 */
static PyObject * py_ejectChunk( PyObject * args )
{
	ToolPtr spTool = ToolManager::instance().tool();

	if (spTool && spTool->locator())
	{
		Vector3 cen = spTool->locator()->transform().applyToOrigin();
		Chunk * pChunk = ChunkManager::instance().cameraSpace()->
			findChunkFromPoint( cen );
		if (pChunk != NULL)
		{
			pChunk->loose( false );
			pChunk->eject();

			Py_Return;
		}
	}

	PyErr_SetString( PyExc_ValueError, "BigBang.ejectChunk() "
		"could not find the chunk to eject." );
	return NULL;
}
PY_MODULE_FUNCTION( ejectChunk, BigBang )


#include "item_view.hpp"
#include "gizmo/general_properties.hpp"
#include "gizmo/current_general_properties.hpp"

/**
 * Move all the current position properties to the given locator
 *
 * Doesn't add a barrier, it's left up to the python code to do that
 */
static PyObject * py_moveGroupTo( PyObject * args )
{
	// get args
	PyObject * pPyLoc;
	if (!PyArg_ParseTuple( args, "O", &pPyLoc ) ||
		!ToolLocator::Check( pPyLoc ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.moveGroupTo() "
			"expects a ToolLocator" );
		return NULL;
	}

	ToolLocator* locator = static_cast<ToolLocator*>( pPyLoc );

	//Move all group objects relatively by an offset.
	//The offset is a relative, snapped movement.
	Vector3 centrePos = CurrentPositionProperties::centrePosition();
	Vector3 locPos = locator->transform().applyToOrigin();
	Vector3 newPos = locPos - centrePos;
	SnapProvider::instance()->snapPositionDelta( newPos );

	Matrix offset;
	offset.setTranslate( newPos );

	std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();
	for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		Matrix m;
		(*i)->pMatrix()->recordState();
		(*i)->pMatrix()->getMatrix( m );

		m.postMultiply( offset );

		if ( BigBang::instance().terrainSnapsEnabled() )
		{
			Vector3 pos( m.applyToOrigin() );
			//snap to terrain only
			pos = Snap::toGround( pos );
			m.translation( pos );
		}
		else if ( BigBang::instance().obstacleSnapsEnabled() )
		{
			Vector3 normalOfSnap = SnapProvider::instance()->snapNormal( m.applyToOrigin() );
			Vector3 yAxis( 0, 1, 0 );
			yAxis = m.applyVector( yAxis );

			Vector3 binormal = yAxis.crossProduct( normalOfSnap );

			normalOfSnap.normalise();
			yAxis.normalise();
			binormal.normalise();

			float angle = acosf( yAxis.dotProduct( normalOfSnap ) );

			Quaternion q( binormal.x * sinf( angle / 2.f ),
				binormal.y * sinf( angle / 2.f ),
				binormal.z * sinf( angle / 2.f ),
				cosf( angle / 2.f ) );

			q.normalise();

			Matrix rotation;
			rotation.setRotate( q );

			Vector3 pos( m.applyToOrigin() );

			m.translation( Vector3( 0.f, 0.f, 0.f ) );
			m.postMultiply( rotation );

			m.translation( pos );
		}

		Matrix worldToLocal;
		(*i)->pMatrix()->getMatrixContextInverse( worldToLocal );

		m.postMultiply( worldToLocal );

		(*i)->pMatrix()->setMatrix( m );
		(*i)->pMatrix()->commitState( false, false );
	}

	Py_Return;
}
PY_MODULE_FUNCTION( moveGroupTo, BigBang )


#include "chunks/editor_chunk.hpp"
static PyObject * py_showChunkReport( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.showChunkReport() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}
	
	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	uint modelCount = 0;

	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	for (; i != items.end(); ++i)
	{
		ChunkItemPtr pItem = *i;
		Chunk* pChunk = pItem->chunk();

		if (pChunk)
		{
			std::vector<DataSectionPtr>	modelSects;
			EditorChunkCache::instance( *pChunk ).pChunkSection()->openSections( "model", modelSects );

			modelCount += (int) modelSects.size();
		}
	}

	char buf[512];
	sprintf( buf, "%d models in selection\n", modelCount );

	Commentary::instance().addMsg( buf );

	Py_Return;
}
PY_MODULE_FUNCTION( showChunkReport, BigBang )


/**
 *	This function sets the current BigBang tool mode
 */
static PyObject * py_setToolMode( PyObject * args )
{
	char* mode = 0;
	if ( !PyArg_ParseTuple( args, "s", &mode ) )
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.setToolMode() "
			"expects a string argument." );
		return NULL;
	}

	if ( mode )
		PanelManager::instance()->setToolMode( mode );

	Py_Return;
}
PY_MODULE_FUNCTION( setToolMode, BigBang )


/**
 *	This function shows or hides a Tool Panel
 */
static PyObject * py_showPanel( PyObject * args )
{
	char* mode = 0;
	int show = -1;
	if ( !PyArg_ParseTuple( args, "si", &mode, &show ) )
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.showPanel() "
			"expects one string and one int arguments." );
		return NULL;
	}

	if ( mode && show != -1 )
		PanelManager::instance()->showPanel( mode, show );

	Py_Return;
}
PY_MODULE_FUNCTION( showPanel, BigBang )


/**
 *	This function asks about a tool panel visibility
 */
static PyObject * py_isPanelVisible( PyObject * args )
{
	char* mode = 0;
	if ( !PyArg_ParseTuple( args, "s", &mode ) )
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.isPanelVisible() "
			"expects a string argument." );
		return NULL;
	}

	if ( mode )
		return PyInt_FromLong( PanelManager::instance()->isPanelVisible( mode ) );

	return NULL;
}
PY_MODULE_FUNCTION( isPanelVisible, BigBang )



/**
 *	AddItemToHistory
 */
static PyObject * py_addItemToHistory( PyObject * args )
{
	char* str = 0;
	char* type = 0;
	if ( !PyArg_ParseTuple( args, "ss", &str, &type ) )
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.addItemToHistory()"
			"expects two string arguments." );
		return NULL;
	}

	if ( str && type )
		PanelManager::instance()->ualAddItemToHistory( str, type );

	Py_Return;
}
PY_MODULE_FUNCTION( addItemToHistory, BigBang )


/**
 *	launchTool
 */
static PyObject* py_launchTool( PyObject * args )
{
	char* name = 0;
	char* cmdline = 0;
	if ( !PyArg_ParseTuple( args, "ss", &name, &cmdline ) )
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.launchTool()"
			"expects two string arguments." );
		return NULL;
	}

	if ( name && cmdline )
	{
		char exe[ MAX_PATH ];
		GetModuleFileName( NULL, exe, sizeof( exe ) );
		if( std::count( exe, exe + strlen( exe ), '\\' ) > 2 )
		{
			*strrchr( exe, '\\' ) = 0;
			*strrchr( exe, '\\' ) = 0;
			std::string path = exe;
			path += "\\";
			path += name;
			std::replace( path.begin(), path.end(), '/', '\\' );

			std::string commandLine = path + "\\" + name + ".exe " + cmdline;

			PROCESS_INFORMATION pi;
			STARTUPINFO si;
			GetStartupInfo( &si );

			TRACE_MSG( "BigBang.launchTool: cmdline = %s, path = %s\n", commandLine.c_str(), path.c_str() );

			if( CreateProcess( NULL, (LPSTR)commandLine.c_str(), NULL, NULL, FALSE, 0, NULL, path.c_str(),
				&si, &pi ) )
			{
				CloseHandle( pi.hThread );
				CloseHandle( pi.hProcess );
			}
		}
	}

	Py_Return;
}
PY_MODULE_FUNCTION( launchTool, BigBang )


// -----------------------------------------------------------------------------
// Section: Common stuff (should be elsewhere...)
// -----------------------------------------------------------------------------

/// Yes, these are globals
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

	float		time;			///< The time of the record.
	PyObject	* function;		///< The function associated with the record.
};

typedef std::priority_queue<TimerRecord>	Timers;
Timers	gTimers;


/**
 *	Registers a callback function to be called after a certain time,
 *	 but not before the next tick. (If registered during a tick
 *	 and it has expired then it will go off still - add a miniscule
 *	 amount of time to BigWorld.time() to prevent this if unwanted)
 *	Non-positive times are interpreted as offsets from the current time.
 */
static PyObject * py_callback( PyObject * args )
{
	float		time = 0.f;
	PyObject *	function = NULL;

	if (!PyArg_ParseTuple( args, "fO", &time, &function ) ||
		function == NULL || !PyCallable_Check( function ) )
	{
		PyErr_SetString( PyExc_TypeError, "py_callback: Argument parsing error." );
		return NULL;
	}

	if (time < 0) time = 0.f;

	//TODO
	//time = EntityManager::getTimeNow() + time;
	Py_INCREF( function );

	TimerRecord		newTR = { time, function };
	gTimers.push( newTR );

	Py_Return;
}
PY_MODULE_FUNCTION( callback, BigBang )


/**
 *	This class implements a PyOutputWriter with the added functionality of
 *	writing to the Python console.
 */
class BWOutputWriter : public PyOutputWriter
{
public:
	BWOutputWriter( const char * prefix, const char * fileText ) :
		PyOutputWriter( fileText, /*shouldWritePythonLog = */true )
	{
	}

protected:
	virtual void printMessage( const char * msg );
};


/**
 *	This method implements the default behaviour for printing a message. Derived
 *	class should override this to change the behaviour.
 */
void BWOutputWriter::printMessage( const char * msg )
{
	XConsole * pXC = ConsoleManager::instance().find( "Python" );
	if (pXC != NULL) 
	{
		pXC->print( msg );
	}

	this->PyOutputWriter::printMessage( msg );
}



// -----------------------------------------------------------------------------
// Section: BigBangScript namespace functions
// -----------------------------------------------------------------------------

/**
 *	This method initialises the BigBang script.
 */
bool BigBangScript::init( DataSectionPtr pDataSection )
{
	std::string scriptPath = "resources/scripts;entities/editor";

	// Call the general init function
	if (!Script::init( scriptPath, "editor" ))
	{
		CRITICAL_MSG( "BigBang::init: Failed to init Script.\n" );
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

	sprintf( fileText, "WorldEditor %s (compiled on %s) starting on %s",
		config,
		aboutCompileTimeString,
		ctime( &aboutTime ) );

	PyObject_SetAttrString( pSysModule,
		"stderr", new BWOutputWriter( "stderr: ", fileText ) );
	PyObject_SetAttrString( pSysModule,
		"stdout", new BWOutputWriter( "stdout: ", fileText ) );

	PyImport_ImportModule( "keys" );

	PyObject * pInit =
		PyObject_GetAttrString( PyImport_AddModule("keys"), "init" );
	if (pInit != NULL)
	{
		PyRun_SimpleString( PyString_AsString(pInit) );
		Py_DECREF( pInit );
	}
	PyErr_Clear();

	return true;
}


/**
 *	This method does the script clean up.
 */
void BigBangScript::fini()
{
	Script::fini();
}
