/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/*
 *
 * Class: TerrainEditor
 *
 */

#include "pch.hpp"

#include "terrain_editor.hpp"
#include "../application_input.hpp"
#include "../module_manager.hpp"
#include "../big_bang.hpp"
#include "../romp_harness.hpp"
#include "../initialisation.hpp"
#include "terrain_locator.hpp"
#include "terrain_view.hpp"
#include "terrain_functor.hpp"
#include "../options.hpp"
#include "../closed_captions.hpp"
#include "../../common/undoredo.hpp"
#include "../../common/base_camera.hpp"

#include "ashes/simple_gui.hpp"

#include "cstdmf/dogwatch.hpp"
#include "moo/render_context.hpp"

#include "resmgr/dataresource.hpp"
#include "resmgr/bwresource.hpp"

#include "romp/console_manager.hpp"
#include "romp/xconsole.hpp"

#ifndef CODE_INLINE
#include "terrain_editor.ipp"
#endif

static DogWatch	g_watchUpdateState( "Update State" );
static DogWatch	g_watchViewUpdate( "Update View" );
static DogWatch	g_watchViewDraw( "Draw View" );

DECLARE_DEBUG_COMPONENT2( "Module", 0 )

// -----------------------------------------------------------------------------
// Section: TerrainEditor
// -----------------------------------------------------------------------------

typedef ModuleManager ModuleFactory;

IMPLEMENT_CREATOR(TerrainEditor, Module)

/*
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"

void TerrainEditor::TerrainSnapCamera::update( float dTime, bool activeInputHandler )
{
	ClosestTerrainObstacle collider;

	MouseLookCamera::update( dTime, activeInputHandler );

	Matrix invView( view_ );
	invView.invert();

	Vector3 pos = invView[3];
	Vector3 extent( pos );
	extent.y -= 1000.f;

	ChunkSpace* space = ChunkManager::instance().space(0);
	if ( space )
	{
		space->collide( 
			pos,
			extent,
			collider );
	}

	if ( collider.dist_ > 0.f )
	{
		invView[3].y = pos.y - collider.dist_ + 1.85f;

		invView.invert();
		view_ = invView;
	}
}
*/

/**
 *	Constructor.
 */
TerrainEditor::TerrainEditor() :
	closedCaptions_( NULL ),
	spAlphaTool_( NULL ),
	spEcotypeTool_( NULL )
{
	float s = Options::pRoot()->readFloat( "app/cameraSpeed1", 8.f );
	float t = Options::pRoot()->readFloat( "app/cameraSpeed2", 40.f );

	BigBangCamera::cam().speed( s );
	BigBangCamera::cam().turboSpeed( t );
}


/**
 *	Destructor.
 */
TerrainEditor::~TerrainEditor()
{
}


/**
 *	This method overrides the Module method.
 *
 *	@see Module::onStart
 */
void TerrainEditor::onStart()
{
    bigBang_ = &BigBang::instance();

	//Create the alpha painting tool
	HeightPoleAlphaFunctorPtr spFunctor = new HeightPoleAlphaFunctor;
	spAlphaFunctor_ = spFunctor;
	AlphaGUITextureToolViewPtr spGUIView = new AlphaGUITextureToolView( spFunctor );
	spGUIView_ = spGUIView;

	spAlphaTool_ = new Tool( new TerrainToolLocator,
		new TerrainTextureToolView(
		Options::pRoot()->readString( "tools/alphaTool",
			"resources/maps/gizmo/disc.bmp" ) ),
		spFunctor );

	spChunkVisualisation_ = new TerrainChunkTextureToolView(
		Options::pRoot()->readString( "tools/chunkVisualisation",
			"resources/maps/gizmo/square.bmp" ));

	if ( Options::pRoot()->readBool( "tools/showChunkVisualisation", true ) )
	{
		spAlphaTool_->addView( "chunkVisualisation", spChunkVisualisation_ );
	}

	spAlphaTool_->addView( "alphaGUI", spGUIView_ );
	spGUIView->visible( true );

	spAlphaTool_->size(
		Options::pRoot()->readFloat( "tools/alphaToolSize", 30 ));
	spAlphaTool_->strength(
		Options::pRoot()->readFloat( "tools/alphaToolStrength", 500 ));


	//Create the height filtering functor
	spHeightFunctor_ = new HeightPoleHeightFilterFunctor;

	//Create the ecotypes tools
	spEcotypeTool_ = new Tool(
			new TerrainChunkLocator,
			new TerrainChunkTextureToolView(
				Options::pRoot()->readString( "tools/chunkVisualisation",
				"resources/maps/gizmo/square.bmp" )),
			new HeightPoleEcotypeFunctor );
	spEcotypeTool_->size( 10000 );


	closedCaptions_ = new ClosedCaptions(
		Options::pRoot()->readFloat( "consoles/numMessageLines", 5 ));

	this->onResume( 0 );
}


/**
 *	This method overrides the Module method.
 *
 *	@see Module::onStop
 */
int	TerrainEditor::onStop()
{
	this->onPause();

	XConsole * pConsole = ConsoleManager::instance().find( "Special" );

	if (pConsole != NULL)
	{
		pConsole->clear();
	}

	Py_DECREF( closedCaptions_ );

	return 0;
}


/**
 *	This method overrides the Module method.
 *
 *	@see Module::onPause
 */
void TerrainEditor::onPause()
{
	bigBang_->popTool();

	closedCaptions_->visible( false );
	Commentary::instance().delView( closedCaptions_ );

	::ShowCursor( true );
}


/**
 *	This method overrides the Module method.
 *
 *	@param exitCode		The exitCode of the module that has just left the stack.
 *
 *	@see Module::onResume
 */
void TerrainEditor::onResume( int exitCode )
{
	bigBang_->pushTool( spAlphaTool_ );

	Commentary::instance().addView( closedCaptions_ );
	closedCaptions_->visible( true );

	::ShowCursor( true );
}


/**
 *	This method overrides the FrameworkModule method.
 *
 *	@see FrameworkModule::updateState
 */
bool TerrainEditor::updateState( float dTime )
{
	SimpleGUI::instance().update( dTime );

	closedCaptions_->update( dTime );
	bigBang_->update( dTime );

	BigBangCamera::cam().update( dTime );

	return true;
}


/**
 *	This method overrides the FrameworkModule method.
 *
 *	@see FrameworkModule::render
 */
void TerrainEditor::render( float dTime )
{
	BigBangCamera::cam().render( dTime );

	bigBang_->render();

	SimpleGUI::instance().draw();
}


/**
 *	This method overrides the Module method.
 *
 *	@see Module::handleKeyEvent
 */
bool TerrainEditor::handleKeyEvent( const KeyEvent & event )
{
	bool handled = false;

	//Keys for tools
	ToolPtr spTool = BigBang::instance().tool();

	if ( spTool )
		handled = spTool->handleKeyEvent( event );

	//Keys for module
	if ( !handled && event.isKeyDown() )
	{
		switch ( event.key() )
		{
			case KeyEvent::KEY_F4:
				if ( InputDevices::isAltDown() )
				{
					ModuleManager::instance().pop();
				}
				handled = true;
				break;
			case KeyEvent::KEY_RBRACKET:
				{
					if ( InputDevices::isShiftDown() )
					{
						ToolPtr spTool = BigBang::instance().tool();
						if ( spTool )
						{
							spTool->strength( spTool->strength() * 1.25f );
							std::string caption = "Tool strength ";
							char buf[64];
							sprintf( buf, "%0.1f", spTool->strength() );
							caption += buf;
							BigBang::instance().addCommentaryMsg( caption, 0 );
						}
					}
					else
					{
						ToolPtr spTool = BigBang::instance().tool();
						if ( spTool )
						{							
							spTool->size( spTool->size() * 1.25f );
							std::string caption = "Tool size ";
							char buf[64];
							sprintf( buf, "%0.1f", spTool->size() );
							caption += buf;
							BigBang::instance().addCommentaryMsg( caption, 0 );
						}
					}
				}
				break;
			case KeyEvent::KEY_LBRACKET:
				{
					if ( InputDevices::isShiftDown() )
					{
						ToolPtr spTool = BigBang::instance().tool();
						if ( spTool )
						{							
							spTool->strength( spTool->strength() * 0.8f );
							std::string caption = "Tool strength ";
							char buf[64];
							sprintf( buf, "%0.1f", spTool->strength() );
							caption += buf;
							BigBang::instance().addCommentaryMsg( caption, 0 );
						}
					}
					else
					{
						ToolPtr spTool = BigBang::instance().tool();
						if ( spTool )
						{
							spTool->size( spTool->size() * 0.8f );
							std::string caption = "Tool size ";
							char buf[64];
							sprintf( buf, "%0.1f", spTool->size() );
							caption += buf;
							BigBang::instance().addCommentaryMsg( caption, 0 );
						}
					}
				}
				break;
			case KeyEvent::KEY_F6:
				if ( Options::pRoot()->readBool( "tools/showChunkVisualisation" ) )
				{
					spAlphaTool_->delView(spChunkVisualisation_);
					Options::pRoot()->writeBool( "tools/showChunkVisualisation" , false );
				}
				else
				{
					spAlphaTool_->addView( "chunkVisualisation", spChunkVisualisation_);
					Options::pRoot()->writeBool( "tools/showChunkVisualisation", true );
				}
				break;

			case KeyEvent::KEY_F8:
				BigBang::instance().save();
				break;

			case KeyEvent::KEY_F9:
				{
					ToolPtr curTool = BigBang::instance().tool();

					if ( curTool == spAlphaTool_ )
					{
						if (curTool->functor() == spAlphaFunctor_)
						{
							AlphaGUITextureToolView* pGui =
								static_cast<AlphaGUITextureToolView*>(&*spGUIView_);

							//enter height mode
							curTool->functor( spHeightFunctor_ );
							pGui->visible( false );

							BigBang::instance().addCommentaryMsg(
								"Entering height filter mode.  Press LMB to apply", 0 );
						}
						else
						{
							// don't assert tool is height tool ...
							//  user might have changed it

							//leave height mode
							curTool->functor( spAlphaFunctor_ );

							//enter ecotype mode
							BigBang::instance().pushTool( spEcotypeTool_ );

							BigBang::instance().addCommentaryMsg(
								"Entering ecotype mode.  Press Enter to apply", 0 );
						}
					}
					else
					{
						MF_ASSERT( curTool == spEcotypeTool_ );

						AlphaGUITextureToolView* pGui =
							static_cast<AlphaGUITextureToolView*>(&*spGUIView_);

						//leave ecotype mode
						BigBang::instance().popTool();
						pGui->visible( true );

						BigBang::instance().addCommentaryMsg(
							"Entering alpha mode.  Press LMB to apply", 0 );
					}
				}
				break;
			case KeyEvent::KEY_Z:
				if (!BigBang::instance().tool()->applying() &&
					InputDevices::isKeyDown( KeyEvent::KEY_LCONTROL ))
				{
					if (!InputDevices::isKeyDown( KeyEvent::KEY_LSHIFT ))
					{
						const std::string & what =
							UndoRedo::instance().undoInfo( 0 );
						if (!what.empty()) BigBang::instance().addCommentaryMsg(
							"Undoing: " + what, 1 );

						UndoRedo::instance().undo();
					}
					else
					{
						const std::string & what =
							UndoRedo::instance().redoInfo( 0 );
						if (!what.empty()) BigBang::instance().addCommentaryMsg(
							"Redoing: " + what, 1 );

						UndoRedo::instance().redo();
					}
				}
				break;
			default:
				break;
		}
	}	

	return handled;
}


/**
 *	This method overrides the Module method.
 *
 *	@see Module::handleMouseEvent
 */
bool TerrainEditor::handleMouseEvent( const MouseEvent & event )
{
	bool handled = false;

	//mouse events for the tool
	ToolPtr spTool = BigBang::instance().tool();
	if ( spTool )
	{
		handled = spTool->handleMouseEvent( event );
	}

	//mouse events for the camera
	if ( !handled )
		BigBangCamera::cam().handleMouseEvent( event );

	return handled;
}


// terrain_editor.cpp
