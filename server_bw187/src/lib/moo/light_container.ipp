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

// INLINE void LightContainer::inlineFunction()
// {
// }

namespace Moo
{
	INLINE
	const Colour& LightContainer::ambientColour( ) const
	{
		return ambientColour_;
	}

	INLINE 
	void LightContainer::ambientColour( const Colour& colour )
	{
		ambientColour_ = colour;
	}

	INLINE 
	const DirectionalLightVector& LightContainer::directionals( ) const
	{
		return directionalLights_;
	}

	INLINE
	DirectionalLightVector& LightContainer::directionals( )
	{
		return directionalLights_;
	}

	INLINE
	void LightContainer::addDirectional( DirectionalLightPtr pDirectional )
	{
		MF_ASSERT( pDirectional );
		directionalLights_.push_back( pDirectional );
	}

	INLINE
	uint32 LightContainer::nDirectionals( ) const
	{
		return directionalLights_.size();
	}

	INLINE
	DirectionalLightPtr LightContainer::directional( uint32 i ) const
	{
		MF_ASSERT( directionalLights_.size() > i );
		return directionalLights_[ i ];
	}

	INLINE
	const SpotLightVector& LightContainer::spots( ) const
	{
		return spotLights_;
	}

	INLINE
	SpotLightVector& LightContainer::spots( )
	{
		return spotLights_;
	}

	INLINE
	void LightContainer::addSpot( SpotLightPtr pSpot )
	{
		MF_ASSERT( pSpot );
		spotLights_.push_back( pSpot );
	}

	INLINE
	uint32 LightContainer::nSpots( ) const
	{
		return spotLights_.size();
	}

	INLINE
	SpotLightPtr LightContainer::spot( uint32 i ) const
	{
		MF_ASSERT( spotLights_.size() > i );
		return spotLights_[ i ];
	}

	INLINE
	const OmniLightVector& LightContainer::omnis( ) const
	{
		return omniLights_;
	}

	INLINE
	OmniLightVector& LightContainer::omnis( )
	{
		return omniLights_;
	}

	INLINE
	void LightContainer::addOmni( OmniLightPtr pOmni )
	{
		MF_ASSERT( pOmni );
		omniLights_.push_back( pOmni );
	}

	INLINE
	uint32 LightContainer::nOmnis( ) const
	{
		return omniLights_.size();
	}

	INLINE
	OmniLightPtr LightContainer::omni( uint32 i ) const
	{
		MF_ASSERT( omniLights_.size() > i );
		return omniLights_[ i ];
	}
}

/*light_container.ipp*/
