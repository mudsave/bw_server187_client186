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

#pragma warning(disable:4786)	// turn off truncated identifier warnings

#include "romp/font.hpp"
#include "math/boundbox.hpp"

#include "text_gui_component.hpp"
#include "simple_gui.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/memory_counter.hpp"
#include "moo/effect_material.hpp"
#include "moo/shader_manager.hpp"

DECLARE_DEBUG_COMPONENT2( "2DComponents", 0 )


#ifndef CODE_INLINE
#include "text_gui_component.ipp"
#endif


memoryCounterDeclare( gui );


PY_TYPEOBJECT( TextGUIComponent )

PY_BEGIN_METHODS( TextGUIComponent )
	/*~ function TextGUIComponent.reset
	 *
	 *	This method redraws the TextGUIComponent if it has become dirty.  It
	 *	shouldn't be neccessary to call this function.
	 */
	PY_METHOD( reset )
	/*~ function TextGUIComponent.stringWidth
	 *
	 *	This function returns the width (in pixels) that the specified string
	 *	will take when rendered to the TextGUIComponent using its current
	 *	setting font and pixelSnap settings.
	 *
	 *	@param text	The string to measure the length of.
	 *
	 *	@return		An integer, the rendered width of the string in pixels.
	 */
	PY_METHOD( stringWidth )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( TextGUIComponent )
	/*~ attribute TextGUIComponent.text
	 *
	 *	The text to be displayed in the component.  It will be rendered using
	 *	the font specified in the font attribute.  The component will be
	 *	resized to fit the text, if a new string is assigned to it.  It doesn't
	 *	wrap text, so if the string is too long to fit on the screen it will
	 *	just extend off the edges.
	 *
	 *	If multiple lines of text are desired, then multiple TextGUIComponents
	 *	are required.
	 *
	 *	@type	String
	 */
	PY_ATTRIBUTE( text )
	/*~ attribute TextGUIComponent.font
	 *
	 *	This attribute specifies the path to the font file that is used to
	 *	render the text.  Assigning to it causes the new font file to be
	 *	loaded. If an invalid file is specified, then a Python Error occurs.
	 *
	 *	By convention, font files are stored in the fonts subdirectory.  They
	 *	are an xml file describing which image to obtain the characters from,
	 *	and the size and location of each character.
	 *
	 *	The default font is default_medium.font".
	 *
	 *	For example:
	 *	@{
	 *	tx = GUI.Text( "test" )
	 *	tx.font = "default_small.font"
	 *	@}
	 *	This example applies the default_small font to the text
	 *
	 *	@type	String
	 */
	PY_ATTRIBUTE( font )
	/*~ attribute TextGUIComponent.pixelSnap
	 *
	 *	This attribute specifies that characters should be aligned exactly onto
	 *	pixels, rather than using the usual texture mapping approach.  This
	 *	forces characters to be properly formed, however the text will be
	 *	slightly offset from where texture mapping may have put it.  It
	 *	defaults to 1 (true).
	 *
	 *	@type	Integer
	 */
	PY_ATTRIBUTE( pixelSnap )
	/*~ attribute TextGUIComponent.explicitSize
	 *
	 *	This attribute specifies whether the text component will resize itself
	 *	to fit exactly within the given text dimensions, or whether the
	 *	size of the component automatically resizes to reflect the best
	 *	on-screen size (default)
	 *
	 *	If this flag is turned off (default) the text component resizes itself
	 *	such that exactly one texel equals one pixel on-screen, for the
	 *	clearest possible text.  In this instance, width and height are read-only
	 *	properties, and are correct after the text label is set, or the
	 *	screen resolution has changed.
	 *
	 *	If this flag is turned on, the text component will draw itself into
	 *	the given dimensions.  This may result in text with an incorrect aspect
	 *	ratio.  In this instance, width and height are read/write.
	 *
	 *  When the flag is turned on, setting either the width or the height to
	 *	0 will maintain the correct aspect ratio.  After setting the width or
	 *	height to 0, the width and height attributes will update to reflect
	 *	the actual size of the text.
	 *
	 *	When the flag is turned on, setting both width and height to 0 will
	 *	set the text to the optimal size (one texel = one pixel).  In order
	 *	to do this, the width and height must be set at the same time, i.e.
	 *	you must use textComponent.size = (0,0)	 
	 *
	 *	@type	Integer
	 */
	PY_ATTRIBUTE( explicitSize )
PY_END_ATTRIBUTES()

/*~ function GUI.Text
 *
 *	This function creates a new TextGUIComponent.  This component renders a
 *	line of text.
 *
 *	@param	text	A string containing the line of text to be rendered.
 *					This can be changed once the component is created.
 *
 *	@return			the newly created component.
 */
PY_FACTORY_NAMED( TextGUIComponent, "Text", GUI )

COMPONENT_FACTORY( TextGUIComponent )



TextGUIComponent::TextGUIComponent( FontPtr f, PyTypePlus * pType )
:SimpleGUIComponent( "", pType ),
 dirty_( false ),
 label_( L"TextGUIComponent" ),
 mesh_( NULL ),
 font_( f ),
 pixelSnap_( true ),
 drawOffset_( 0.f, 0.f, 0.f ),
 explicitSize_( false )
{
	mesh_ = new CustomMesh<GUIVertex>;
	if ( f && f->pTexture() )
		SimpleGUIComponent::textureName( f->pTexture()->resourceID() );
	materialFX( FX_BLEND );

	this->widthInClipCoords( true );
	this->heightInClipCoords( true );
}


TextGUIComponent::~TextGUIComponent()
{
	if (mesh_)
		delete mesh_;
}


/// Get an attribute for python
PyObject * TextGUIComponent::pyGetAttribute( const char * attr )
{
	// if it's dirty, recalculate it so the caller gets the correct value
	if ( dirty_ )
	{
		reset();
	}

	PY_GETATTR_STD();

	return SimpleGUIComponent::pyGetAttribute( attr );
}


/// Set an attribute for python
int TextGUIComponent::pySetAttribute( const char * attr, PyObject * value )
{
	// set dirty_ to true, to force a recalc of the text resources. It
	// forces the recalc always to be on the safe side.
	dirty_ = true;

	PY_SETATTR_STD();

	return SimpleGUIComponent::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * TextGUIComponent::pyNew( PyObject * args )
{
	PyObject * label = NULL;
	char * fontName = NULL;
	wchar_t wBuf[256];
	if (!PyArg_ParseTuple( args, "|Os", &label, &fontName ))
	{
		PyErr_SetString( PyExc_TypeError, "GUI.Text: "
			"Argument parsing error: Expected an optional text string (ansi or unicode) and optional font name" );
		return NULL;
	}

	if (label && PyUnicode_Check(label))
	{
		int numChars = PyUnicode_AsWideChar( (PyUnicodeObject*)label, wBuf, 255 );
		if ( numChars != -1 )
			wBuf[numChars] = L'\0';
	}
	else if (label && PyString_Check(label))
	{
		wsprintfW( wBuf, L"%S\0", PyString_AsString(label) );
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "GUI.Text: "
			"Argument parsing error: Text string must be of type String or Unicode" );
		return NULL;
	}

	FontPtr pFont = fontName == NULL ? s_defFont() : FontManager::instance().get( fontName );
	if ( !pFont )
		pFont = s_defFont();
	TextGUIComponent* pText = new TextGUIComponent(	pFont );
	pText->label( wBuf );

	// all set, now force a recalc of the object
	pText->dirty_ = true;

	return pText;
}


PyObject* TextGUIComponent::py_reset( PyObject * args )
{
	if ( dirty_ )
	{
		reset();
	}

	Py_Return;
}


void TextGUIComponent::reset()
{
	this->recalculate();
	dirty_ = false;

	//and move and colour the vertices.
	//we have to do this here in case this was called from
	//a script call that was itself called during the update
	//of another gui component.
	//
	//i.e. TimerText callsback a script in its update, which
	//in turn adds an alert, which calls otherText.text = blah
	//and otherText.reset().  In this case, the otherText needs
	//a fully realised vertex buffer for when it draws (it may
	//have already had update() called )
	this->copyAndMove();
}


void
TextGUIComponent::update( float dTime )
{
	// turn off mip mapping here (nowhere else to do it)
	//material_.textureStage(0).useMipMapping( false );
	runTimeColour( colour() );
	runTimeTransform( Matrix::identity );

	if ( SimpleGUI::instance().hasResolutionChanged() )
	{
		if (!explicitSize_) dirty_ = true;
	}

	if ( dirty_ )
	{
		recalculate();	//note this calls copyAndMove.
		dirty_ = false;
	}

	this->copyAndMove();

	SimpleGUIComponent::updateChildren( dTime );
}


/**
 *	This method copies the blueprint vertices to the run-time vertices and
 *	moves them into place.
 */
void TextGUIComponent::copyAndMove()
{
	if ( nVertices_ )
	{
		//copy to temporary buffer
		memcpy( vertices_, blueprint_, nVertices_ * sizeof( GUIVertex ) );

		//move temporary vertices to current position
		float x,y,w,h;

		if (mesh_)
		{
			w = this->SimpleGUIComponent::width();
			h = this->SimpleGUIComponent::height();
		}
		else
		{
			w = vertices_[nVertices_-2].pos_.x - vertices_[0].pos_.x;
			h = vertices_[0].pos_.y - vertices_[3].pos_.y;
		}
		this->layout( position().x, position().y, w, h, x, y );

		drawOffset_.x = ( x - vertices_[0].pos_.x );
		drawOffset_.y = ( y - vertices_[0].pos_.y );

		if ( pixelSnap_ )
		{
			//assuming the text was placed on a pixel boundary, make sure the offset
			//does not move off the pixel boundary.
			float xp = 1.f / Moo::rc().halfScreenWidth();
			float yp = 1.f / Moo::rc().halfScreenHeight();

			drawOffset_.x = xp * floorf(drawOffset_.x / xp);
			drawOffset_.y = yp * floorf(drawOffset_.y / yp);
		}

		for ( int i = 0; i < nVertices_; i++ )
		{
			// TODO: get rid of this as well - draw text into the correct spot in the first place.
			vertices_[i].pos_.x += drawOffset_.x;
			vertices_[i].pos_.y += drawOffset_.y;
			vertices_[i].pos_.z = position().z;
		}
	}
}


/**
 *	This method recalculates the text mesh.
 *	After calling this method, the width and height of the component will be correct.
 */
void TextGUIComponent::recalculate()
{
	SimpleGUIComponent::cleanMesh();

	mesh_->clear();

	float w = 0.0f;
	float h = 0.0f;

	if ( font_ )
	{		
		if (explicitSize_)
		{
			font_->drawIntoMesh( *mesh_, label_, 0.f, 0.f, width_, height_, &w, &h );						
		}
		else
		{
			font_->drawIntoMesh( *mesh_, label_, 0.f, 0.f, &w, &h );
		}		
		SimpleGUIComponent::width( w );
		SimpleGUIComponent::height( h );
		calculateMeshSize();
		nVertices_ = label_.length() * 6;
		blueprint_ = new GUIVertex[nVertices_];
		vertices_ = new GUIVertex[nVertices_];
		memoryCounterAdd( gui );
		memoryClaim( blueprint_ );
		memoryClaim( vertices_ );

	//note xbox needs no indices, since is draws with quads.
		// TODO: use indices again if the real cause for the flickering problem
		// is found and fixed, or remove it once and for all
		// No longer using indices, since the new 'drawIntoMesh' generates the
		// vertices in the correct order to render with drawPrimitiveUP. The 
		// previous method of rendering with drawIndexedPrimitiveUP was generating
		// an unexplained flickering of text on nVidia cards.
		//nIndices_ = label_.length() * 6;
		//indices_ = new uint16[nIndices_];
		//memoryClaim( indices_ );
		//memcpy( indices_, font_->indices().s_indices, nIndices_ * sizeof( uint16 ) );

		//now, rip out the font vertices
		if (mesh_->size() > 0)
		{
			memcpy( blueprint_, &mesh_->front(),  mesh_->size() * sizeof( (*mesh_)[0] ) );
		}
	}
	else
	{
		nVertices_ = 0;
	}
}


void TextGUIComponent::calculateMeshSize()
{
	BoundingBox bb( Vector3(0,0,0), Vector3(0,0,0) );

	for ( int i = 0; i < mesh_->nVerts(); i++ )
	{
		bb.addBounds( reinterpret_cast<Vector3&>((*mesh_)[i].pos_) );
	}

	meshSize_ = bb.maxBounds() - bb.minBounds();
}


/**
 *	This static method gets a default XFont object
 */
FontPtr TextGUIComponent::s_defFont()
{
	static FontPtr sxf = FontManager::instance().get( "default_medium.font" );
	return sxf;
}

/**
 *	Load
 */
bool TextGUIComponent::load( DataSectionPtr pSect, LoadBindings & bindings )
{
	if (!this->SimpleGUIComponent::load( pSect, bindings )) return false;

	this->label( pSect->readWideString( "label", this->label() ) );
	this->font( pSect->readString( "font", this->font() ) );
	this->pixelSnap( pSect->readBool( "pixelSnap", this->pixelSnap() ) );
	this->explicitSize_ = pSect->readBool( "explicitSize", this->explicitSize_ );

	return true;
}

/**
 *	Save
 */
void TextGUIComponent::save( DataSectionPtr pSect, SaveBindings & bindings )
{
	this->SimpleGUIComponent::save( pSect, bindings );

	pSect->writeWideString( "label", this->label() );
	pSect->writeString( "font", this->font() );
	pSect->writeBool( "pixelSnap", this->pixelSnap() );
	pSect->writeBool( "explicitSize", this->explicitSize_ );
}


/**
 *	This method sets the text item's font.
 *
 *	@param	fontName	The name of the font file.
 */
void TextGUIComponent::font( const std::string& fontName )
{
	FontPtr pFont = FontManager::instance().get( fontName );
	if ( !pFont )
	{
		ERROR_MSG( "font %s unknown\n", fontName.c_str() );
	}
	else
	{
		font_ = pFont;
		if ( font_->pTexture() )
		{
			SimpleGUIComponent::textureName( font_->pTexture()->resourceID() );
			dirty_ = true;
		}
	}
}


/**
 *	This method returns the font name.
 *
 *	@return	std::string		The name of the font
 */
const std::string TextGUIComponent::font() const
{
	if ( font_ )
	{
		return FontManager::instance().findFontName( font_ );
	}
	else
	{
		return FontManager::instance().findFontName( s_defFont() );
	}
}


static bool settingDimensions = false;

void TextGUIComponent::size( const Vector2 & size )
{
	if (settingDimensions) return;
	//do this so we don't possibly recalculate.
	//this is so if explicitSize_ is true, we don't 
	//do 2 recalculations.
	settingDimensions = true;
	SimpleGUIComponent::size(size);
	settingDimensions = false;
	//and this sparks a possible recalculate
	this->width(size.x);
}


void TextGUIComponent::width( float w )
{
	SimpleGUIComponent::width(w);
	if (settingDimensions) return;		
	settingDimensions = true;
	if (explicitSize_)
	{
		this->recalculate();
	}
	settingDimensions = false;
}


void TextGUIComponent::height( float h )
{	
	SimpleGUIComponent::height(h);
	if (settingDimensions) return;
	settingDimensions = true;
	if (explicitSize_)
	{
		this->recalculate();
	}	
	settingDimensions = false;
}

// text_gui_component.cpp
