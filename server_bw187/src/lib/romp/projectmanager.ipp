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

// INLINE void ProjectManager::inlineFunction()
// {
// }

INLINE SceneRoot* ProjectManager::scene( void )
{
	return scene_;
}
/*
INLINE HeightMapTerrain* ProjectManager::terrain( void )
{
	return terrain_;
}*/

#ifdef EDITOR_ENABLED
INLINE bool ProjectManager::isSaved( void )
{
	return !changed_ && terrain_->saved( ) && !scene_->hasChanged( );
}

INLINE std::string ProjectManager::name( void )
{
	return name_;
}

#endif




/*projectmanager.ipp*/
