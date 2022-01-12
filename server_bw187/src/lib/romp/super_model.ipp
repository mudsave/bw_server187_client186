/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// super_model.ipp

#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif


/**
 *	Calculate the blend ratio for this action
 */
INLINE float SuperModelAction::blendRatio() const
{
	float fsum = min( (passed) / pSource->blendInTime_,
		finish > 0.f ? (finish - passed) / pSource->blendOutTime_ : 1.f );
	return fsum >= 1.f ? 1.f : fsum > 0.f ? fsum : 0.f;
}


// super_model.ipp
