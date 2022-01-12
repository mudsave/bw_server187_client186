/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// baseappmgr.ipp

#ifdef CODE_INLINE
#define INLINE    inline
#else
#define INLINE
#endif

/**
 *	This static method returns the singleton instance of this class.
 */
INLINE
BaseAppMgr & BaseAppMgr::instance()
{
	MF_ASSERT( pInstance_ );
	return *pInstance_;
}


/**
 *	This static method returns the singleton instance of this class.
 */
INLINE
BaseAppMgr * BaseAppMgr::pInstance()
{
	return pInstance_;
}

// baseappmgr.ipp
