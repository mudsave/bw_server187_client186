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

#include "projectmanager.hpp"
#include <io.h>

#include "resmgr/bwresource.hpp"
#include "rhino2/light.hpp"
#include "moo/terrain.hpp"
#include "xactsnd/soundmgr.hpp"
#include "xactsnd/soundloader.hpp"
#include "time_of_day.hpp"
#include "sky_gradient_dome.hpp"
#include "sun_and_moon.hpp"
#include "water.hpp"
#include "sea.hpp"


#ifndef CODE_INLINE
#include "projectmanager.ipp"
#endif



ProjectManager::ProjectManager() :
  scene_( NULL ),
  changed_( false ),
#ifdef EDITOR_ENABLED
  lockFH_( NULL ),
#endif
  ambientColour_( 128.f, 128.f, 128.f ),
  backgroundColour_( 0.f, 128.f, 255.f ),
  fogColour_( 255.f, 128.f, 0.f ),
  detailInnerRadius_( 100 ),
  detailOuterRadius_( 200 )
{
	scene_		= new SceneRoot;

    fogStart_ = 5000;
    fogEnd_ = 10000;
    fogDensity_ = 1.0f;
    fogEnabled_ = false;
}

ProjectManager::~ProjectManager()
{
#ifdef EDITOR_ENABLED
    releaseLock( );
#endif

	if (scene_ != NULL) delete scene_;
}

void ProjectManager::ensureProjectFilesExist( const std::string name, int width, int height )
{
#ifdef EDITOR_ENABLED
	std::string prjFile = BWResource::resolveFilename( "projects\\" + name + "\\project.prj" );

	BWResource::ensurePathExists( BWResource::getFilePath( prjFile ) );

	// create the new terrain file, if one doesn't exist
	if ( !BWResource::fileExists( BWResource::resolveFilename( "projects\\" + name + "\\terrain.ter" ) ) )
    {
	    terrain_->newTerrain( "projects\\" + name + "\\terrain.ter", width, height );
    	//terrain_->renameTerrain( BWResource::resolveFilename(  ) );
		terrain_->saveTerrain( );
        changed_ = true;
    }

	// create the new scene file, if one doesn't exist
	if ( !BWResource::fileExists( BWResource::resolveFilename( "projects\\" + name + "\\scene.mfo" ) ) )
    {
        changed_ = true;
        scene_->clear( );
		scene_->save( "projects\\" + name + "\\scene.mfo" );
    }

    // create the project file if one doesn't exist
	if ( !BWResource::fileExists( BWResource::resolveFilename( "projects\\" + name + "\\project.prj" ) ) )
    {
        changed_ = true;
	    save( name, true );
    }
#endif
}


bool ProjectManager::load( const std::string projectName, int width, int height, ProgressDisplay * pPrg )
{
	bool result = false;

    scene_->clear( );

	ensureProjectFilesExist( projectName, width, height );

    std::string file = "projects\\" + projectName + "\\project.prj";

#ifdef EDITOR_ENABLED
	// release the old previous lock
    releaseLock( );

	lockFile_ = "projects\\" + projectName + "\\lock.txt";
    lockFile_ = BWResource::resolveFilename( lockFile_ );

    // try deleting the old lock.txt file.
    // If someone has the project open this will fail to delete the lock file, and
    // we will by in read-only mode.
    releaseLock( );
#endif


	ProgressTask projectTask( pPrg, "Loading Project " + projectName, 8 );

	name_ = projectName;

	DataSectionPtr spRoot = BWResource::instance().rootSection()->openSection( file );

    if ( spRoot )
    {
		projectTask.step();

		nodeCount_ = -1;
		DataSectionPtr spSettings = spRoot->openSection( "Editor", true );

		if ( spSettings )
		{
			nodeCount_ = spSettings->readInt( "count" );
		}

		DataSectionPtr spTerrain = spRoot->openSection( "Terrain" );

		std::string tFile = "projects\\" + projectName + "\\terrain.ter";


		// load terrain
		if (spTerrain)
			Moo::Terrain::instance().detailTypes().loadMappings( spTerrain );

		if (BWResource::fileExists( tFile ))
			Moo::Terrain::instance().openTerrain( tFile, pPrg );

        if ( spTerrain )
        {
			// read detail texture information
			detailInnerRadius_ = spTerrain->readInt( "detailInnerRadius", 100 );
			detailOuterRadius_ = spTerrain->readInt( "detailOuterRadius", 200 );
			detailTextureFilename_ = spTerrain->readString( "detailTexture" );
        }

		// set terrain options
//			terrain_->setLOD( HIGHEST_LOD );
//			terrain_->updateCamera( true );

		//TODO: add detail accessors
		//terrain_->detailRadiusOuter( detailOuterRadius_ );
		//terrain_->detailRadiusInner( detailInnerRadius_ );

		/*if ( detailTextureFilename_.compare( "" ) != 0 )
			terrain_->setDetailBitmap( detailTextureFilename_ );*/

		std::string sFile = "projects\\" + projectName + "\\scene.mfo";

		projectTask.step();

		// load the existing file
		result = scene_->load( sFile, pPrg, nodeCount_ );

        spSettings = spRoot->openSection( "background", true );

        if ( spSettings )
        {
            backgroundColour_ = Vector3(spSettings->readVector3( "colour" ));

            backgroundColour( backgroundColour_ );
        }

		projectTask.step();

	    spSettings = spRoot->openSection( "ambientlight", true );

        if ( spSettings )
        {
        	ambientColour_ = Vector3(spSettings->readVector3( "colour" ));

			ambientColour( ambientColour_ );                
        }

		projectTask.step();

        spSettings = spRoot->openSection( "fog", true );

        if ( spSettings )
        {
            fogEnabled_ = spSettings->readBool( "enabled" );
            fogColour_ = Vector3(spSettings->readVector3( "colour" ));
            fogStart_ = spSettings->readInt( "start" );
            fogEnd_ = spSettings->readInt( "end" );
            fogDensity_ = spSettings->readFloat( "density" );

            setFog( fogColour_, fogStart_, fogEnd_, fogDensity_ );
            enableFog( fogEnabled_ );
        }

		projectTask.step();
		// Load the sounds
		DataSectionPtr sndSect = spRoot->openSection("soundList");
		if (sndSect) {
			ProgressTask task( pPrg, "Loading Sounds", sndSect->countChildren());
			DataSection::iterator it;

			for (it=sndSect->begin();  it != sndSect->end();  it++)
			{
				DataSectionPtr dsp = *it;
				std::string file = dsp->asString();
				if (soundMgr().isInited() && !file.empty()) {
					DataSectionPtr dsp = BWResource::openSection(file);
					if (dsp)
						SoundLoader::loadFromXML(dsp);
				}
				task.step();
			}
		}

		//Load the waters
		spSettings = spRoot->openSection( "waters" );
		if (spSettings)
			Waters::instance().loadWaters( spSettings );

		projectTask.step();


        // check for project lock file.
#ifdef EDITOR_ENABLED
        checkForLock( );
#endif
		projectTask.step();
    }

	// load all the environment stuff
	enviro_.init( spRoot );

	projectTask.step();

	// tell our scene to load that timeOfDay object
	useTimeOfDaySun( );

    name_ = projectName;

    changed_ = false;

	return result;
}

void ProjectManager::useTimeOfDaySun( void )
{
	scene_->outsideLighting( &enviro_.timeOfDay()->lighting() );
}


void ProjectManager::reloadTimeOfDay( void )
{
    std::string file = "projects\\" + name_ + "\\project.prj";

    std::string pfile = file;

    if ( BWResource::fileExists( pfile ) )
    {
        DataResource prjFile( pfile, RESOURCE_TYPE_XML );

        DataSectionPtr spRoot = prjFile.getRootSection( );

        if ( spRoot )
        {
			enviro_.timeOfDay()->load( spRoot );
		}
	}
}


#ifdef EDITOR_ENABLED
bool ProjectManager::save( const std::string projectName, bool infoOnly, ProgressDisplay * pPrg )
{
	bool result = false;

	if ( !locked( ) )
    {
        std::string file = BWResource::resolveFilename( "projects\\" + projectName + "\\project.prj" );

        DataResource prjFile( file, RESOURCE_TYPE_XML );

        DataSectionPtr spRoot = prjFile.getRootSection( );

        if ( spRoot )
        {
			ProgressTask	projectTask( pPrg, "Saving project " + projectName, 7 );

            if ( !infoOnly )
            {
                incrementRevisions( projectName );
            }


            DataSectionPtr spTerrain = spRoot->openSection( "Terrain", true );

			projectTask.step();

            if ( spTerrain )
            {
                // save detail texture information
                spTerrain->writeInt( "detailInnerRadius", detailInnerRadius_ );
                spTerrain->writeInt( "detailOuterRadius", detailOuterRadius_ );

                if ( detailTextureFilename_.compare( "" ) != 0 )
                    spTerrain->writeString( "detailTexture", detailTextureFilename_ );
            }

            if ( !infoOnly && !terrain_->saved( ) )
            {
                std::string tFile = BWResource::resolveFilename( "projects\\" + projectName + "\\terrain.ter" );
                terrain_->renameTerrain( tFile );
                terrain_->saveTerrain( pPrg );
            }

			projectTask.step();

            if ( !infoOnly && scene_->hasChanged( ) )
			{
                scene_->save( "projects\\" + projectName + "\\scene.mfo", pPrg );
			}

            DataSectionPtr spSettings = spRoot->openSection( "background", true );

			projectTask.step();

            if ( spSettings )
            {
                spSettings->writeVector3( "colour", backgroundColour_ );
            }

            spSettings = spRoot->openSection( "ambientlight", true );

            if ( spSettings )
                spSettings->writeVector3( "colour", ambientColour_ );

            spSettings = spRoot->openSection( "fog", true );

            if ( spSettings )
            {
                spSettings->writeBool( "enabled", fogEnabled_ );
                spSettings->writeVector3( "colour", fogColour_ );
                spSettings->writeInt( "start", fogStart_ );
                spSettings->writeInt( "end", fogEnd_ );
                spSettings->writeFloat( "density", fogDensity_ );
            }

			projectTask.step();

			if ( timeOfDay_->file( ) == "" )
			{
				spRoot->deleteSection( "TimeOfDay" );
				timeOfDay_->save( spRoot );
				skyGradientDome_.save( spRoot );
			}
			else
			{
				spSettings = spRoot->openSection( "Environment", true );

				if ( spSettings )
					spSettings->writeString( "file", timeOfDay_->file( ) );

				timeOfDay_->save( );
				skyGradientDome_.save( timeOfDay_->file( ) );
			}

			projectTask.step();

			spSettings = spRoot->openSection( "Editor", true );

			if ( spSettings )
			{
				spSettings->writeInt( "count", scene_->count( ) );
			}

			projectTask.step();

            prjFile.save( );

            if ( !infoOnly )
                saveRevision( projectName );

            name_ = projectName;
            result = true;

			projectTask.step();
        }
	}

    changed_ = false;

	return result;
}

bool ProjectManager::save( ProgressDisplay* pPrg, bool infoOnly )
{
	bool result = false;

	if ( name_ != "" )
		result = save( name_, infoOnly, pPrg );

    changed_ = false;

	return result;
}

void ProjectManager::incrementRevisions( const std::string projectName )
{
	SHFILEOPSTRUCT shfo;
    char czNewFile[4096];
    char czOldFile[4096];

	std::string revPath = BWResource::resolveFilename( "project-revisions\\" + projectName + "\\" );

	// get the last revision files list
	StringVector list;

	findAllFiles( list, revPath, "*.r9" );

	// and delete them
	deleteFiles( list );

	// rename all revisions up one
	std::string rev = ".rx";
	StringVector nList;

	for ( int i = 8; i >= 0; i-- )
	{
		// check this works???
		rev[2] = '0' + i;

		findAllFiles( list, revPath, rev );

		// build the new file names
		while( list.size() )
		{
			std::string oldFile = *(*list.begin());
			std::string newFile = *(*list.begin());

			newFile[newFile.length()-1] = '0' + i + 1;

            // copy the filenames into the char arrays
            ZeroMemory( czOldFile, 4096 );
            ZeroMemory( czNewFile, 4096 );
            CopyMemory( czOldFile, oldFile.c_str(), oldFile.size() );
            CopyMemory( czNewFile, newFile.c_str(), newFile.size() );

            ZeroMemory( &shfo, sizeof( SHFILEOPSTRUCT ) );

            // setup for renaming the file
            shfo.hwnd  = NULL;
            shfo.wFunc = FO_RENAME;
            shfo.pFrom = czOldFile;
            shfo.pTo   = czNewFile;
            shfo.fFlags= FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;

            // renmae the file
			int iRC = SHFileOperation( &shfo );

			// clear the string
			delete *list.begin();

			list.erase( list.begin() );
		}
	}
}

void ProjectManager::deleteFiles( StringVector& list )
{
	// delete all the old revision files
	while( list.size() )
	{
		std::string file = *(*list.begin());

		DeleteFile( file.c_str() );

		delete *list.begin();

		list.erase( list.begin() );
	}
}

void ProjectManager::findAllFiles( StringVector& list, std::string path, std::string pattern )
{
	struct _finddata_t fileinfo;
	long hFile;

	std::string wildPath = path + "*.*";

    if( ( hFile = _findfirst( (char*)wildPath.c_str(), &fileinfo )) != -1L )
	{
       	do
		{
			if ( fileinfo.attrib & _A_SUBDIR )
			{
            	if ( fileinfo.name[0] != '.' )	// don't recurse into [.]current or [..]parent directories
                {
					// recurse into the sub-directories
					std::string p( fileinfo.name );
					p = path + p + "\\";
					findAllFiles( list, p, pattern );
                }
			}
			else
			{
            	if ( strcmp( fileinfo.name, "lock.txt" ) != 0 )
                {
                    // check name extension
                    bool correctExtension = true;
                    std::string cFile = fileinfo.name;
                    int pl = pattern.size();
                    int cl = cFile.size();

                    for ( int i = 0; i < pl && correctExtension; i++ )
                        correctExtension = !( pattern[pl - i] != cFile[cl - i] );

                    if ( correctExtension )
                    {
                        // if correct add to the list
                        std::string f = fileinfo.name;
                        std::string* newFile = new std::string( path + f );

                        list.push_back( newFile );
                    }
                }
			}

		} while ( _findnext( hFile, &fileinfo ) != -1L );

		_findclose( hFile );
	}
}

void ProjectManager::saveRevision( const std::string projectName )
{
	SHFILEOPSTRUCT shfo;
    char czNewFile[4096];
    char czOldFile[4096];

	// copy all the projects file to the revision directory
	std::string prjPath = BWResource::resolveFilename( "projects\\" + projectName + "\\" );

	StringVector list;

	findAllFiles( list, prjPath, "" );

	// copy all files to the new project-revisions directory with the .r0 extension added to all
	while ( list.size( ) )
	{
		// replace the 'projects' with 'project-revision'
		std::string oldFile = BWResource::dissolveFilename( *(*list.begin()) );
		std::string newFile = oldFile.substr( oldFile.find_first_of( "projects\\", 0 ) + 9, oldFile.size() );

		oldFile = BWResource::resolveFilename( oldFile );
		newFile = BWResource::resolveFilename( "project-revisions\\" + newFile + ".r0" );

        // make sure the path we are copying to, exists.
		BWResource::ensurePathExists( newFile );

        // copy the filenames into the char arrays
        ZeroMemory( czOldFile, 4096 );
        ZeroMemory( czNewFile, 4096 );
        CopyMemory( czOldFile, oldFile.c_str(), oldFile.size() );
        CopyMemory( czNewFile, newFile.c_str(), newFile.size() );

        // copy the file
        ZeroMemory( &shfo, sizeof( SHFILEOPSTRUCT ) );

        shfo.hwnd  = NULL;
        shfo.wFunc = FO_COPY;
        shfo.pFrom = czOldFile;
        shfo.pTo   = czNewFile;
        shfo.fFlags= FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;

        // copy the file
        int iRC = SHFileOperation( &shfo );

        delete *list.begin();

        list.erase( list.begin() );
	}
}

void ProjectManager::restoreRevision( int revision )
{
	SHFILEOPSTRUCT shfo;
    char czNewFile[4096];
    char czOldFile[4096];

	std::string prjPath = BWResource::resolveFilename( "projects\\" + name_ + "\\" );
	std::string revPath = BWResource::resolveFilename( "project-revisions\\" + name_ + "\\" );

	StringVector list;

	// get ALL the lastest project files
	findAllFiles( list, prjPath, "" );

	// and delete them ALL
	deleteFiles( list );

    std::string rev = ".rx";
    rev[2] = '0' + revision;

    // get all the revision files
	findAllFiles( list, revPath, rev );

    // copy all the revision files to the project directory
	while ( list.size( ) )
	{
		// replace the 'project-revision' with 'projects'
		std::string oldFile = BWResource::dissolveFilename( *(*list.begin()) );
		std::string newFile = oldFile.substr( oldFile.find_first_of( "project-revisions\\", 0 ) + 18, oldFile.size() );

		oldFile = BWResource::resolveFilename( oldFile );
		newFile = BWResource::resolveFilename( "projects\\" + newFile.substr( 0, newFile.size() - 3 ) );

        // make sure the path we are copying to, exists.
		BWResource::ensurePathExists( newFile );

        // copy the filenames into the char arrays
        ZeroMemory( czOldFile, 4096 );
        ZeroMemory( czNewFile, 4096 );
        CopyMemory( czOldFile, oldFile.c_str(), oldFile.size() );
        CopyMemory( czNewFile, newFile.c_str(), newFile.size() );

        // copy the file
        ZeroMemory( &shfo, sizeof( SHFILEOPSTRUCT ) );

        shfo.hwnd  = NULL;
        shfo.wFunc = FO_COPY;
        shfo.pFrom = czOldFile;
        shfo.pTo   = czNewFile;
        shfo.fFlags= FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;

        // copy the file
        int iRC = SHFileOperation( &shfo );

        delete *list.begin();

        list.erase( list.begin() );
	}

    // reload the project
	load( name_ );
}

#endif


void ProjectManager::backgroundColour( Vector3 colour )
{
	backgroundColour_ = colour;

//    if ( renderContext_ )
//        renderContext_->setBackgroundColour( colour );

    changed_ = true;
}

Vector3 ProjectManager::backgroundColour( void )
{
	return backgroundColour_;
}

void ProjectManager::ambientColour( Vector3 colour )
{

	//TODO: ambient colour

	ambientColour_ = colour;

#ifndef EDITOR_ENABLED
//    if ( renderContext_ )
//    {
//        Light* light = renderContext_->getAmbientLight();
//
//        if ( light )
//        	light->getColour( ) = colour;
//    }
#endif

    changed_ = true;
}

Vector3 ProjectManager::ambientColour( void )
{
	return ambientColour_;
}

void ProjectManager::setFog( Vector3 colour, int start, int end, float density )
{
	// TODO: fog
    fogColour_ = colour;
    fogStart_ = start;
    fogEnd_ = end;
    fogDensity_ = density;

/*    if ( renderContext_ )
	    renderContext_->setFog(((uint32) fogColour_[2] << 16) |
                             ((uint32) fogColour_[1] << 8) |
                             ((uint32) fogColour_[0]),
                                       fogStart_, fogEnd_, fogDensity_, 0 );*/
    changed_ = true;
}

void ProjectManager::getFog( Vector3& colour, int& start, int& end, float& density )
{
//	if ( renderContext_ )
	{
/*		unsigned long tcolour;
		float tstart;
		float tend;
		float tdensity;
		char ttype;

		//TODO:: more fog
//		renderContext_->getFog( tcolour, tstart, tend, tdensity, ttype );

		fogColour_ = Vector3( ( tcolour & 0x000000FF ), ( tcolour & 0x0000FF00 ) >> 8, ( tcolour & 0x00FF0000 ) >> 16 );
		fogStart_ = tstart;
		fogEnd_ = tend;
		fogDensity_ = tdensity;*/
	}

    colour = fogColour_;
    start = fogStart_;
	end = fogEnd_;
    density = fogDensity_;
}

void ProjectManager::enableFog( bool state )
{
	fogEnabled_ = state;

	//TODO: add global fog state
//    if ( renderContext_ )
//	    renderContext_->setFogEnabled( fogEnabled_ );
    changed_ = true;
}

bool ProjectManager::fogEnabled( void )
{
	return fogEnabled_;
}

#ifdef EDITOR_ENABLED
void ProjectManager::checkForLock( void )
{
	if ( BWResource::fileExists( lockFile_ ) )
    {
		lock( );
    }
    else
    {
		unlock( );

        // create a lock file to stop others changing the project
        lockFH_ = fopen( lockFile_.c_str(), "w" );

        if ( lockFH_ )
        {
	        fprintf( lockFH_, "The existence of this file prevents others from changing the project while they are editing it.\nDelete this file if no has BigBang! open.\n" );
            fflush( lockFH_ );
        }
    }
}

void ProjectManager::releaseLock( void )
{
	if ( lockFH_ )
    {
    	fclose( lockFH_ );
        lockFH_ = NULL;
    }

    if ( lockFile_ != "" )
        DeleteFile( lockFile_.c_str( ) );

    unlock( );
}
#endif

std::ostream& operator<<(std::ostream& o, const ProjectManager& t)
{
	o << "ProjectManager\n";
	return o;
}

// projectmanager.cpp
