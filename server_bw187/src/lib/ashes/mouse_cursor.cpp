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

#include "mouse_cursor.hpp"
#include "simple_gui.hpp"
#include "moo/render_context.hpp"
#include "moo/texture_manager.hpp"
#include "resmgr/auto_config.hpp"

DECLARE_DEBUG_COMPONENT2( "MouseCursor", 0 );

namespace { // anonymous

// Helper functions
DX::Surface * getTextureSurface( Moo::BaseTexturePtr texture );

// Named constants
// Note: changes made to the AutoConfig parameters below must be also made in
// bigworld/src/tools/res_packer/packer_helper.cpp.
const AutoConfigString s_cursorDefFile( 
		"gui/cursorsDefinitions", 
		"gui/mouse_cursors.xml" );

} // namespace anonymous

// -----------------------------------------------------------------------------
// section: MouseCursor
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( MouseCursor )

PY_BEGIN_METHODS( MouseCursor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( MouseCursor )
	/*~ attribute MouseCursor.position
	 *
	 *	Current position of the mouse cursor (in clip space). 
	 *	Read and write property.
	 *
	 *	@type	Vector2
	 */
	PY_ATTRIBUTE( position )
	/*~ attribute MouseCursor.shape
	 *
	 *	Currently active cursor shape. Read and write property. 
	 *
	 *	Possible values are defined in the mouse cursor definition file.
	 *
	 *	@type	str
	 */
	PY_ATTRIBUTE( shape )
	/*~ attribute MouseCursor.visible
	 *
	 *	Mouse cursor current visibility status. Read and write property.
	 *
	 *	@type	boolean
	 */
	PY_ATTRIBUTE( visible )
	/*~ attribute MouseCursor.active
	 *
	 *	Mouse cursor current activity status. Will be true if the 
	 *	mouse cursor is the current active cursor (see BigWorld.setCursor).
	 *	Read only property.
	 *
	 *	@type	boolean
	 */
	PY_ATTRIBUTE( active )
PY_END_ATTRIBUTES()


/**
 *	This method returns the singleton instance of the class.
 */
MouseCursor & MouseCursor::instance()
{
	static MouseCursor* pInstance;
	if (pInstance == NULL) 
	{
		pInstance = new MouseCursor();
	}
	return *pInstance;
}


/**
 *	Constructor.
 */
MouseCursor::MouseCursor( PyTypePlus * pType ) :
	InputCursor( pType ),
	inactivePos_( 0, 0 ),
	isActive_( false ),
	isVisible_( false ),
	isDirty_( false ),
	cursorDefs_(),
	curCursor_()
{
	this->loadCursorsDefinitions();
	curCursor_ = cursorDefs_.begin();
}


/**
 *	Destructor.
 */
MouseCursor::~MouseCursor()
{}


/**
 *	Returns mouse cursor's current position.
 */
Vector2 MouseCursor::position() const
{
	if (!this->isActive_)
	{
		return this->inactivePos_;
	}
	
	//NEW
	POINT pt;
	::GetCursorPos( &pt );
	::ScreenToClient( (HWND)SimpleGUI::instance().hwnd(), &pt );

	float x, y;
	SimpleGUI::instance().pixelRangesToClip( float(pt.x), float(pt.y), &x, &y );
		
	return Vector2( x - 1.0f, 1.0f - y );
}


/**
 *	Sets a new position for the mouse cursor.
 */
void MouseCursor::position( const Vector2 & pos )
{
	float x, y;
	SimpleGUI::instance().clipRangesToPixel( 
		float(pos.x + 1.0f), 
		float(1.0f - pos.y), 
		&x, &y );

	POINT pt;
	pt.x = LONG( x );
	pt.y = LONG( y );
	::ClientToScreen( (HWND)SimpleGUI::instance().hwnd(), &pt );
	::SetCursorPos( pt.x, pt.y );
	
	if (Moo::rc().device()) 
	{
		Moo::rc().device()->SetCursorPosition( pt.x, pt.y, 0 );
	}
	
	this->inactivePos_ = pos;
}


/**
 *	Returns current mouse cursor shape.
 */
const std::string & MouseCursor::shape() const
{
	if (curCursor_ != this->cursorDefs_.end())
	{
		return curCursor_->first;
	}
	else 
	{
		static std::string emptyStr( "" );
		return emptyStr;
	}
}


/**
 *	Sets a new shape for the mouse cursor.
 */
void MouseCursor::shape( const std::string & cursorName )
{
	CursorDefMap::const_iterator cursor = this->cursorDefs_.find( cursorName );
	if (this->setShape( cursor ))
	{
		this->curCursor_ = cursor;
	}
	else
	{
		ERROR_MSG( "Could not change cursor shape: %s\n", cursorName.c_str() );
		PyErr_PrintEx(0);
	}
}


/**
 *	Sets the visibility status of the mouse cursor.
 */
bool MouseCursor::visible() const
{
	return this->isVisible_;
}


/**
 *	Sets the visibility status of the mouse cursor.
 */
void MouseCursor::visible( bool state )
{
	if ( Moo::rc().device() )
	{
		Moo::rc().device()->ShowCursor( state );
		if (state) {
			// increase cursor visibility 
			// counter until cursor is visible
			while (::ShowCursor( TRUE ) <	0) {}
		}
		else {
			// decrease cursor visibility 
			// counter until cursor is invisible
			while (::ShowCursor( FALSE ) > -1) {}
		}
		this->isVisible_ = state;
	// TODO: XBOX implementation
	}
}


/**
 *	Handle key events. The mouse cursor does nothing with them.
 */
bool MouseCursor::handleKeyEvent( const KeyEvent & /*event*/ )
{ 
	return false;
}


/**
 *	Handle mouse events. The mouse cursor does nothing with them.
 */
bool MouseCursor::handleMouseEvent( const MouseEvent & /*event*/ )
{
	if (Moo::rc().device()) 
	{
		POINT pt;
		::GetCursorPos( &pt );
		Moo::rc().device()->SetCursorPosition( pt.x, pt.y, 0 );
	}
	return false;
}


/**
 *	Handle axis events. The mouse cursor does nothing with them.
 */
bool MouseCursor::handleAxisEvent( const AxisEvent & /*event*/ )
{ 
	return false;
}


/**
 *	Ticks the mouse cursor.
 *
 *	@param	dTime	time elapsed in between frames.
 */
void MouseCursor::tick( float dTime )
{
	if (this->isDirty_)
	{
		this->refresh();
	}
	else if (this->isActive_ &&
			 this->curCursor_ != this->cursorDefs_.end() &&
			 this->curCursor_->second.texture->isAnimated())
	{
		// Updating cursor if it's an animated texture
		this->setShape( this->curCursor_ );
	}
}


void MouseCursor::focus( bool focus )
{
	this->isDirty_ = focus;
}

	
bool MouseCursor::isDirty() const
{
	return this->isDirty_;
}

	
void MouseCursor::refresh()
{
	//Only execute if we are using this cursor
	if (this->isActive_ == false)
	{
		return;
	}

	this->setShape( this->curCursor_ );
	this->visible( this->isVisible_ );
	this->isDirty_ = false;
}


/**
 *	Activates the mouse cursor. This method is called automaticaly
 *	by App when the scripts sets the mouse cursor as the active cursor.
 *	Normally, there should be no need to call it directly.
 */
void MouseCursor::activate()
{
	this->position( this->inactivePos_ );
	this->isActive_ = true;
}


/**
 *	Activates the mouse cursor. This method is called automaticaly
 *	by App when the scripts unsets the mouse cursor as the active 
 *	cursor. Normally, there should be no need to call it directly.
 */
void MouseCursor::deactivate()
{
	this->inactivePos_ = this->position();
	this->isActive_ = false;
}


/**
 *	Returns mouse cursor's current activity status 
 */
bool MouseCursor::isActive() const 
{ 
	return this->isActive_;
}


/**
 *	Loads cursor definitions based on the information in the 
 *	cursor defition xml file. The name of the cursor definition xml 
 *	resource file can be set in the resources.xml file, under the field
 *	gui/cursorsDefinitions field. If defaults to "gui/mouse_cursors.xml"
 */
void MouseCursor::loadCursorsDefinitions()
{
	DataSectionPtr pTop = BWResource::openSection( s_cursorDefFile );
	this->cursorDefs_.erase( this->cursorDefs_.begin(), this->cursorDefs_.end() );
	if (pTop)
	{
		for (int i=0; i<pTop->countChildren(); ++i) 
		{
			CursorDefinition cursorDef;
			DataSectionPtr pSect = pTop->openChild( i );
			std::string name     = pSect->sectionName();
			std::string fileName = pSect->readString( "texture" );
			cursorDef.hotSpot    = pSect->readVector2( "hotSpot", Vector2( 0, 0 ) );
			Moo::TextureManager::instance()->setFormat(fileName, D3DFMT_A8R8G8B8);
			cursorDef.texture = Moo::TextureManager::instance()->get(fileName);
			if (cursorDef.texture->format() != D3DFMT_A8R8G8B8)
			{
				CRITICAL_MSG(
					"Cursor texture maps need to be of D3DFMT_A8R8G8B8 format to work.\n"
					"Make sure the DDS file is saved as D3DFMT_A8R8G8B8 or that the\n"
					"original TGA is available for corversion to D3DFMT_A8R8G8B8.");
			}

			this->cursorDefs_.insert( std::make_pair( name, cursorDef ) );
		}
	}
}

/**
 * Sets shape of mouse cursor.
 *
 * @param	curIt iterator to entry into the cursor definitions map.
 * @return	true is cursor was successfully set. False on error.
 */
bool MouseCursor::setShape( const CursorDefMap::const_iterator & curIt )
{
	if ( curIt != this->cursorDefs_.end() )
	{
		DX::Surface * surface = getTextureSurface( curIt->second.texture );
		if (surface != NULL)
		{	
			HRESULT hr = Moo::rc().device()->SetCursorProperties( 
					int( curIt->second.hotSpot.x ), 
					int( curIt->second.hotSpot.y ), 
					surface );
			surface->Release();
	
			if ( SUCCEEDED( hr ) )
			{
				return true;
			}
			else
			{
				ERROR_MSG( 
						"MouseCursor::setShape: "
						"Could not set cursor properties: %s\n", 
						curIt->first );
				PyErr_PrintEx(0);
			}
		}
	}
	return false;
}


/**
 * Callback triggered by Moo when the device is reset. Flag cursor as dirty.
 */
void MouseCursor::createUnmanagedObjects()
{
	this->isDirty_ = true;
}

	
/**
 * Callback triggered by Moo when the device is recreated. Flag cursor as dirty.
 */
void MouseCursor::createManagedObjects()
{
	this->isDirty_ = true;
}


/**
 *	Get an attribute for python
 */
PyObject * MouseCursor::pyGetAttribute( const char * attr )
{
	// try our normal attributes
	PY_GETATTR_STD();

	// ask our base class
	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int MouseCursor::pySetAttribute( const char * attr, PyObject * value )
{
	// try our normal attributes
	PY_SETATTR_STD();

	// ask our base class
	return PyObjectPlus::pySetAttribute( attr, value );
}

/*~ function GUI.mcursor
 *
 *	This function returns the MouseCursor object. 
 *
 *	Use it, in conjunction with BigWorld.setCursor function to activate the 
 *	mouse cursor. The mouse cursor object should be activated whenever is 
 *	desirable to have the user interacting with GUI components by using the 
 *	mouse. No mouse events will be propagated to the components if the mouse 
 *	cursor is not active. To activate the mouse cursor, use:
 *
 *	@{
 *		BigWorld.setCursor( GUI.mcursor() )
 *	@}
 *
 *	Also, use it to access and modify all mouse cursor properties, like 
 *	position, visible, shape and active:
 *	
 *	@{
 *		mouseStatus = GUI.mcursor().active
 *		mousePosition = GUI.mcursor().position
 *		GUI.mcursor().visible = True
 *		GUI.mcursor().shape = 'arrow'
 *	@}
 */
/**
 * Returns the mouse cursor singleton as a PyObject.
 */
PyObject * MouseCursor::py_mcursor( PyObject * args )
{
	MouseCursor * pMC = &MouseCursor::instance();
	Py_INCREF( pMC );
	return pMC;
}
PY_MODULE_STATIC_METHOD( MouseCursor, mcursor, GUI )


// Helper functions
namespace { // anonymous

/**
 * Return level 0 surface from given texture.
 *
 * @param	texture		texture from where to extract surface.
 * @return	Pointer to surface on success. NULL on error.
 */
DX::Surface * getTextureSurface( Moo::BaseTexturePtr texture )
{
	DX::Surface * surface = NULL;
	if ( texture.getObject() != NULL && texture->pTexture() != NULL )
	{
		DX::BaseTexture * baseTexture = texture->pTexture();
		D3DRESOURCETYPE resType = baseTexture->GetType();
		if ( resType == D3DRTYPE_TEXTURE )
		{
			DX::Texture* tSrc = (DX::Texture*)baseTexture;
			tSrc->GetSurfaceLevel( 0, &surface );
		}
	}
	else 
	{
		ERROR_MSG( "getTextureSurface: Null or invalid texture object\n" );
		PyErr_PrintEx(0);
	}
	return surface;
}

} // namespace anonymous

// mouse_cursor.cpp
