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

#include "resmgr/datasection.hpp"
#include "moo/render_target.hpp"
#include "romp/time_of_day.hpp"

class ChunkPhotographer : public Aligned
{
public:
	static bool photograph( class Chunk& );

	//These methods should be called by the application.
	//If not, no big deal you'll just get memory leaks etc.
	static bool init( DataSectionPtr pSection );
	static void finz();
private:
	static ChunkPhotographer& instance();
	ChunkPhotographer();

	bool takePhoto(class Chunk&);
	void beginScene();
	void setStates(class Chunk&);
	void renderScene();
	void resetStates();
	void endScene();

	Moo::RenderTargetPtr			pRT_;
	int								width_;
	int								height_;
	static Moo::LightContainerPtr	s_lighting_;
	static OutsideLighting			s_chunkLighting_;
	Moo::LightContainerPtr			savedLighting_;
	OutsideLighting*				savedChunkLighting_;
	float							oldFOV_;
	Matrix							oldInvView_;
};