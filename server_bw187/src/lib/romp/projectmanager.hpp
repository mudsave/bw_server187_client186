/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PROJECTMANAGER_HPP
#define PROJECTMANAGER_HPP

#pragma warning ( disable: 4503 )

#include <iostream>

#include "scene2/scene.hpp"
#include "scene2/scene_root.hpp"
#include "progress.hpp"

#include "enviro_minder.hpp"
class Seas;
class Waters;

typedef std::vector< std::string* > StringVector;

class ProjectManager
{
public:
	ProjectManager();
	~ProjectManager();

	bool				load( const std::string projectName, int width = 512, int height = 512, ProgressDisplay * pPrg = NULL );
#ifdef EDITOR_ENABLED
	bool				save( const std::string projectName, bool infoOnly = false, ProgressDisplay * pPrg = NULL );
	bool				save( ProgressDisplay * pPrg, bool infoOnly = false );
	bool				isSaved( void );
	std::string			name( void );
	void 				restoreRevision( int revision );
    bool				locked( void ) 	{ return locked_;  };
    void				lock( void ) 	{ locked_ = true;  };
    void				unlock( void ) 	{ locked_ = false; };
	std::string&		lockFile( void ) { return lockFile_; };
#endif

	SceneRoot*			scene( void );

    void				backgroundColour( Vector3 colour );
    Vector3				backgroundColour( void );

    void				ambientColour( Vector3 colour );
    Vector3				ambientColour( void );

	void 				setFog( Vector3 colour, int start, int end, float density );
	void 				getFog( Vector3& colour, int& start, int& end, float& density );
    void				enableFog( bool enabled );
	bool 				fogEnabled( void );

	EnviroMinder &		enviro()				{ return enviro_; }
	TimeOfDay*			timeOfDay( void )		{ return enviro_.timeOfDay(); };
	SkyGradientDome*	skyGradientDome( void )	{ return enviro_.skyGradientDome(); }
	SunAndMoon*			sunAndMoon( void )		{ return enviro_.sunAndMoon(); }

	Seas*				seas()					{ return enviro_.seas(); }

	void				useTimeOfDaySun( void );
	void				reloadTimeOfDay( void );

private:

	SceneRoot*			scene_;
	EnviroMinder		enviro_;

	std::string			name_;
#ifdef EDITOR_ENABLED
	std::string			lockFile_;
    FILE*				lockFH_;
#endif

    Vector3				ambientColour_;
    Vector3				backgroundColour_;
    Vector3				fogColour_;
    int					fogStart_;
    int					fogEnd_;
    float				fogDensity_;
    bool				fogEnabled_;

    bool				locked_;

	int					detailInnerRadius_;
	int					detailOuterRadius_;
	std::string			detailTextureFilename_;

    bool				changed_;

	int					nodeCount_;

	void				ensureProjectFilesExist( const std::string name, int width, int height );
#ifdef EDITOR_ENABLED
	void 				incrementRevisions( const std::string projectName );
	void 				saveRevision( const std::string projectName );
	void				findAllFiles( StringVector& list, std::string path, std::string pattern );
	void				deleteFiles( StringVector& list );
	void 				checkForLock( void );
	void 				releaseLock( void );
#endif

	ProjectManager(const ProjectManager&);
	ProjectManager& operator=(const ProjectManager&);

	friend std::ostream& operator<<(std::ostream&, const ProjectManager&);
};

#ifdef CODE_INLINE
#include "projectmanager.ipp"
#endif




#endif
/*projectmanager.hpp*/