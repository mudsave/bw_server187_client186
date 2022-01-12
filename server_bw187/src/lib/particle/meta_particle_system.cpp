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

#pragma warning(disable: 4786)
#pragma warning(disable: 4503)

#include "meta_particle_system.hpp"
#include "particle/actions/particle_system_action.hpp"
#include "particle/actions/source_psa.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/diary.hpp"
#include "cstdmf/bgtask_manager.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"

DECLARE_DEBUG_COMPONENT2( "Particle", 0 )


// -----------------------------------------------------------------------------
// Section: Constructor(s) and Destructor.
// -----------------------------------------------------------------------------

/**
 *	This function checks to see if the provided file is a particle system.
 *
 *	@param file	The file to the potential particle system.
 */
/*static*/ bool MetaParticleSystem::isParticleSystem( const std::string& file )
{
	DataSectionPtr ds = BWResource::openSection( file );
	if ( !ds )
		return false; // file is not a datasection or doesn't exist

	// hardcoding particle's section names because of a lack of better way at 
	// the moment:
	for( int i = 0; i < ds->countChildren(); ++i )
	{
		DataSectionPtr child = ds->openChild( i );
		// finding standard sections in the file
		if ( child->sectionName() == "seedTime" ||
			child->findChild( "serialiseVersionData" ) != NULL )
		{
			return true;
		}
		
		// finding alternate section as done in ParticleSystem::load
		DataSectionPtr firstDS = child->openFirstSection();
		if ( firstDS != NULL &&
			firstDS->findChild( "serialiseVersionData" ) != NULL )
		{
			return true;
		}
	}

	return false;
}


/**
 *	This is the constructor for MetaParticleSystem.
 *
 *	@param pType			Parameters passed to the parent PyObject class.
 */
MetaParticleSystem::MetaParticleSystem():
	pOwnWorld_( NULL ),
	attached_( false ),
	inWorld_( false )
{
}


/**
 *	This is the destructor for ParticleSystem.
 */
MetaParticleSystem::~MetaParticleSystem()
{
	removeAllSystems();
}


MetaParticleSystemPtr MetaParticleSystem::clone() const
{
    MetaParticleSystemPtr result = new MetaParticleSystem();		
    for (size_t i = 0; i < systems_.size(); ++i)
    {
        result->addSystem(systems_[i]->clone());
    }
    return result;
}


/**
 *	This static method returns all of the resources that will be required to
 *	create the meta particle system.  It is designed so that the resources
 *	can be loaded in the loading thread before constructing the particle system
 */
/* static */ void MetaParticleSystem::prerequisites( DataSectionPtr pDS, std::set<std::string>& output )
{
	if (pDS)
	{
		if ( pDS->readString( "serialiseVersionData", "" ) != "" )
		{
			//is a particle system
			ParticleSystem::prerequisites( pDS, output );
		}
		else
		{
			//is a meta-particle system	
			for(DataSectionIterator it = pDS->begin(); it != pDS->end(); it++)
			{
				DataSectionPtr pSystemDS = *it;
				std::string systemName = pSystemDS->sectionName();
				if (systemName != "seedTime")
				{
					ParticleSystem::prerequisites( pSystemDS, output );					
				}
			}
		}
	}	
}


bool MetaParticleSystem::load( const std::string& filename,
							  const std::string& directory )
{
	DataSectionPtr pDS = BWResource::openSection(directory + filename);
	return this->load(pDS, BWResource::removeExtension(filename));
}


bool MetaParticleSystem::load(DataSectionPtr pDS, const std::string & name)
{
	if (!pDS)
		return false;

	bool ok = true;	

	// make sure empty
	removeAllSystems();

	if (pDS->openSection( "serialiseVersionData" ))
	{
		ParticleSystemPtr newSystem = new ParticleSystem();

		// this is in old format, there is only one system
		// read in the system properties
		ok = newSystem->load(pDS);
		newSystem->name(name);
		addSystem( newSystem );
	}
	else
	{
		// loop through the system descriptions
		for(DataSectionIterator it = pDS->begin(); it != pDS->end(); it++)
		{
			DataSectionPtr pSystemDS = *it;
			std::string systemName = pSystemDS->sectionName();
			if (systemName != "seedTime")
			{
				ParticleSystemPtr newSystem = new ParticleSystem;

				// read in the system properties
				ok &= newSystem->serialise(pSystemDS, true, systemName);
				addSystem( newSystem );
			}
		}
	}

	// Call this as we may be attached before loading; we still want to
	// maintain the attachement to the constituent particle systems	
	MatrixLiaison* attachment = pOwnWorld_;
	if (attachment)
	{
		this->detach();
		this->attach( attachment );
	}

	return ok;
}


void MetaParticleSystem::save( const std::string& filename, 
    const std::string& directory, bool transient )
{
	// always save to the new format
	DataSectionPtr directoryDS = BWResource::openSection(directory, true); // create a new directory if need to
	DataSectionPtr pDS = directoryDS->openSection(filename, true);	// create a new section if need to

	// for now, just remember if there was a seed time in the file
	float seedTime = pDS->readFloat( "seedTime", -1.f );

	// delete children (as some may have been deleted by the user)
	pDS->delChildren();

	// loop through the systems defined
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		// save system properties in separate sections
		ParticleSystemPtr system = *it;
		std::string systemName = system->name();

		DataSectionPtr systemDS = pDS->openSection(systemName, true);
		system->serialise(systemDS, false, systemName, transient);
	}

	if ( seedTime > -1.f )
	{
		pDS->writeFloat( "seedTime", seedTime );
	}

	// save cache to file
	pDS->save();
}


// reSerialise always serialised from the particle system's perspective
// i.e. does not make new actions / systems etc.
// (as opposed to the DataSectionPerspective when doing load)
// it is used by undo/redo in the editor
void MetaParticleSystem::reSerialise( DataSectionPtr pDS, bool load, bool transient)
{
	if (load)
	{
		//remove systems that no longer exist.
		ParticleSystems::iterator iter = systems_.begin();
		while ( iter != systems_.end() )
		{
			ParticleSystemPtr system = *iter;
			std::string systemName = system->name();

			if ( !pDS->findChild(systemName) )
			{
				iter = systems_.erase(iter);
			}
			else
			{
				iter++;
			}
		}

		//add new systems
		DataSectionIterator it;
		for (it = pDS->begin(); it != pDS->end(); it++)
		{
			DataSectionPtr pSystemDS = *it;
			std::string systemName = pSystemDS->sectionName();
			ParticleSystemPtr ps = this->system( systemName.c_str() );
			if (!ps)
			{
				ps = new ParticleSystem();
				this->addSystem( ps );
			}
			ps->serialise(pSystemDS, load, systemName, transient);
		}
	}
	else
	{
		for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
		{
			// save system properties in separate sections
			ParticleSystemPtr system = *it;
			std::string systemName = system->name();

			DataSectionPtr systemDS = pDS->openSection(systemName, !load);
			MF_ASSERT(systemDS);

			// TODO: make a reSerialise for each system component
			system->serialise(systemDS, load, systemName, transient);
		}
	}
}


void MetaParticleSystem::addSystem(ParticleSystemPtr system)
{
	if (attached_)
	{
		system->detach();
		system->attach(pOwnWorld_);
	}

	systems_.push_back(system);
}


void MetaParticleSystem::insertSystem(size_t idx, ParticleSystemPtr system)
{
	if (attached_)
	{
		system->detach();
		system->attach(pOwnWorld_);
	}

	systems_.insert(systems_.begin() + idx, system);
}


void MetaParticleSystem::removeSystem(ParticleSystemPtr system)
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		if (*it == system)
		{
			ParticleSystemPtr system = *it;
			system->detach();			
			systems_.erase(it);
			return;
		}
	}
}


void MetaParticleSystem::removeAllSystems()
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		ParticleSystemPtr system = *it;
		system->detach();		
	}
	systems_.clear();
}


ParticleSystemPtr MetaParticleSystem::system(const char * name)
{
	const std::string systemName(name);

	for (ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		if ((*it)->name() == systemName)
			return *it;
	}

	return NULL;
}


ParticleSystemPtr MetaParticleSystem::systemFromIndex( size_t index )
{
	if( index < this->systems_.size() )
		return this->systems_[ index ];
	else
		return NULL;
}


int MetaParticleSystem::size( void )
{
	int numberOfParticles = 0;
	for( ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		if ( (*it)->enabled() )
			numberOfParticles += (*it)->size();
	}
	return numberOfParticles;
}


int MetaParticleSystem::capacity( void )
{
	int capacityOfParticles = 0;
	for( ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		capacityOfParticles += (*it)->capacity();
	}
	return capacityOfParticles;
}


size_t MetaParticleSystem::sizeInBytes( void ) const
{
	size_t footprint = 0;
	for( ParticleSystems::const_iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		if ( (*it)->enabled() )
			footprint += (*it)->sizeInBytes();
	}
	return footprint;
}


/**
 *	Tick the particle systems
 *
 *	@param dTime	Time since last update in seconds.
 */
void MetaParticleSystem::tick( float dTime )
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->tick(dTime);
	}
}


/**
 *	This methods tells the particle system to draw itself.
 *
 *	Particles are stored in world coordinates so we do not need
 *	the attachment transform which is passed in.
 */
void MetaParticleSystem::draw( const Matrix &world, float lod )
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		if ( (*it)->enabled() )
			(*it)->draw(world, lod);
	}
}


/**
 *	This accumulates each systems' bounding box into the given matrix
 */
void MetaParticleSystem::boundingBoxAcc( BoundingBox & bb, bool skinny )
{
	BoundingBox newBB = BoundingBox::s_insideOut_;
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->boundingBoxAcc( newBB, skinny );
	}
	if (newBB != BoundingBox::s_insideOut_)
	{
		bb.addBounds( newBB );
	}
}


void MetaParticleSystem::visibilityBoxAcc( BoundingBox & bb )
{
	BoundingBox newBB = BoundingBox::s_insideOut_;
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->visibilityBoxAcc( newBB );
	}
	if (newBB != BoundingBox::s_insideOut_)
	{
		bb.addBounds( newBB );
	}
}


/**
 *	We have been attached. Tell each particle system
 */
bool MetaParticleSystem::attach( MatrixLiaison * pOwnWorld )
{
	if (attached_) return false;

	attached_ = true;
	pOwnWorld_ = pOwnWorld;
	
	bool ret = true;

	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		ret |= (*it)->attach( pOwnWorld );
	}

	return ret;
}


/**
 *	We have been detached. Tell each particle system
 */
void MetaParticleSystem::detach()
{
	attached_ = false;
	pOwnWorld_ = NULL;

	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->detach();
	}
}


void MetaParticleSystem::enterWorld()
{
	inWorld_ = true;

	for (uint32 i=0; i<systems_.size(); i++)
		systems_[i]->enterWorld();
}


void MetaParticleSystem::leaveWorld()
{
	inWorld_ = false;

	for (uint32 i=0; i<systems_.size(); i++)
		systems_[i]->leaveWorld();
}


void MetaParticleSystem::tossed( bool outside )
{
	for (uint32 i=0; i<systems_.size(); i++)
		systems_[i]->tossed(outside);
}


void MetaParticleSystem::move( float dTime )
{
	for (uint32 i=0; i<systems_.size(); i++)
		systems_[i]->move(dTime);
}


void MetaParticleSystem::spawn( int num )
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		ParticleSystem::Actions & actions =  (*it)->actionSet();

		for (ParticleSystem::Actions::iterator actionIt = actions.begin();
				actionIt != actions.end();
				actionIt++)
		{
			if ((*actionIt)->typeID() == PSA_SOURCE_TYPE_ID)
			{
				SourcePSA * source = (SourcePSA *)((*actionIt).getObject());
				source->force(num);
			}
		}
	}
}


void MetaParticleSystem::clear()
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->clear();
	}
}


void MetaParticleSystem::setDoUpdate()
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->setDoUpdate();
	}
}


void MetaParticleSystem::setFirstUpdate()
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->setFirstUpdate();
	}
}


/*
 *	Advise the particle system that it will always be stationary
 */
void MetaParticleSystem::isStatic( bool s )
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->isStatic( s );
	}
}


/*
 *	Advise the particle system bbox that it has been moved
 *	Only required for static particle systems moved throught the editor
 */
void MetaParticleSystem::transformBoundingBox(const Matrix& trans)
{
	for(ParticleSystems::iterator it = systems_.begin(); it != systems_.end(); it++)
	{
		(*it)->transformBoundingBox( trans );
	}
}


float MetaParticleSystem::duration() const
{
	float duration = -1;
	ParticleSystems::const_iterator it = systems_.begin();
	while( it != systems_.end())
	{
		duration = max( (*it)->duration(), duration );
		++it;
	}

	return duration;
}

uint32 MetaParticleSystem::nSystems() const
{
	return systems_.size();
}


// meta_particle_system.cpp
