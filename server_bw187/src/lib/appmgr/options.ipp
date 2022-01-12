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

// INLINE void Options::inlineFunction()
// {
// }


/**
 *	This method returns the root section of options tree.
 */
INLINE
DataSectionPtr Options::pRoot()
{
	return instance_.pRootSection_;
}


/*options.ipp*/
