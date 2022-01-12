/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TERRAIN_EDITOR_HPP
#define TERRAIN_EDITOR_HPP

#include "../module.hpp"
#include "../tool.hpp"

class ClosedCaptions;

class TerrainEditor : public FrameworkModule
{
public:
	TerrainEditor();
	~TerrainEditor();

	// Overrides
	virtual void onStart();
	virtual int	 onStop();

	virtual void onPause();
	virtual void onResume( int exitCode );

	//the event pump
	virtual bool updateState( float dTime );
	virtual void render( float dTime );

	//input handler
	bool handleKeyEvent( const KeyEvent & /*event*/ );
	bool handleMouseEvent( const MouseEvent & /*event*/ );

private:
	TerrainEditor(const TerrainEditor&);
	TerrainEditor& operator=(const TerrainEditor&);

	class BigBang		*bigBang_;

	ClosedCaptions *	closedCaptions_;

	SmartPointer<Tool>		spAlphaTool_;	
	SmartPointer<ToolView>	spChunkVisualisation_;
	SmartPointer<ToolView>	spGUIView_;
	SmartPointer<Tool>		spEcotypeTool_;
	SmartPointer<ToolFunctor> spAlphaFunctor_;
	SmartPointer<ToolFunctor> spHeightFunctor_;
};

#ifdef CODE_INLINE
#include "terrain_editor.ipp"
#endif




#endif // TERRAIN_EDITOR_HPP
