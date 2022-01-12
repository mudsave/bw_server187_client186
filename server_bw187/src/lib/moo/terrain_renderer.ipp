/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif

namespace Moo
{
	INLINE bool	TerrainRenderer::canSeeTerrain() const
	{
		return isVisible_; // TODO [otso@umbra.fi] added because Umbra needs to flush terrain rendering earlier and blocks_ can be empty here
		//return (bool)(blocks_.size() != 0);
	}
	INLINE void TerrainRenderer::clear()
	{
		//Use this method if you are not drawing terrain
		blocks_.clear();
		isVisible_ = false;
	}
} //namespace Moo

// terrain_renderer.ipp