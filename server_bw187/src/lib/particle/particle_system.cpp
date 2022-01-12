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

#include "particle_system.hpp"
#include "particle/actions/particle_system_action.hpp"
#include "particle/actions/source_psa.hpp"
#include "particle/actions/sink_psa.hpp"
#include "particle/renderers/particle_system_renderer.hpp"
#include "particle/renderers/mesh_particle_renderer.hpp"
#include "particle/renderers/visual_particle_renderer.hpp"
#include "particle/renderers/sprite_particle_renderer.hpp"
#include "romp/super_model.hpp"	// for Model::s_blendCookie_
#include "romp/lens_effect_manager.hpp"
//PCWJ TODO
//#include "romp/lod_settings.hpp"
#include "moo/render_context.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/memory_trace.hpp"
#include "cstdmf/memory_counter.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/win_file_system.hpp"

DECLARE_DEBUG_COMPONENT2( "Duplo", 0 )

#ifndef CODE_INLINE
#include "particle_system.ipp"
#endif


// -----------------------------------------------------------------------------
// Section: Static Variable for Particle System Actions.
// -----------------------------------------------------------------------------
bool ParticleSystem::active_ = true;
Vector3 Particle::s_velocity( 0.f,0.f,0.f );
Vector3 ParticleSystem::windVelocity_( 0.0f, 0.0f, 0.0f );
int ParticleSystem::s_counter_ = 0;

// -----------------------------------------------------------------------------
// Section: Constructor(s) and Destructor.
// -----------------------------------------------------------------------------

memoryCounterDefine( particles, Entity );

/*static*/ uint32 ParticleSystem::s_idCounter_ = 0;

/**
 *	This is the constructor for ParticleSystem.
 *
 *	@param initialCapacity	The initial limit to the amount of particles
 *							accepted by the system.
 *	@param pType			Parameters passed to the parent PyObject class.
 */
ParticleSystem::ParticleSystem( int initialCapacity ) :	
	windFactor_( 0.f ),
	enabled_( true ),
	doUpdate_( false ),
	firstUpdate_( false ),
	pGS_( NULL ),
	pRenderer_( NULL ),
	particles_( new ContiguousParticles ),
	counter_( s_counter_++ % 10 ),
	boundingBox_( BoundingBox::s_insideOut_ ),
	maxBB_( BoundingBox::s_insideOut_ ),
	explicitPosition_( 0,0,0 ),
	explicitDirection_( 0,0,1 ),
	explicitTransform_( false ),
	localOffset_( 0.f, 0.f, 0.f ),
	maxLod_( 0.f ),	
	fixedFrameRate_( -1.f ),
	framesLeftOver_( 0.f ),
	static_( false ),	
	id_(s_idCounter_++),
	forcingSave_( false ),
	pOwnWorld_( NULL ),
	attached_( false ),
	inWorld_( false )
{	
	this->capacity( min(initialCapacity,MAX_CAPACITY) );
}

/**
 *	This is the destructor for ParticleSystem.
 */
ParticleSystem::~ParticleSystem()
{
	Actions::iterator iter = actions_.begin();

	while ( iter != actions_.end() )
	{
		ParticleSystemActionPtr pAction = *iter;
		iter = actions_.erase( iter );		
	}

	this->pRenderer( NULL );	
}

ParticleSystemPtr ParticleSystem::clone() const
{
    ParticleSystemPtr result = new ParticleSystem();

    // Remove const for saving:
    ParticleSystem *myself = const_cast<ParticleSystem *>(this);
    XMLSectionPtr tempSection = new XMLSection("temp");
    myself->serialise(tempSection, false, name(), true);
    result->serialise(tempSection, true, name(), true);
	result->enabled( myself->enabled() );
    return result;
}

bool ParticleSystem::load( std::string filename, std::string directory )
{
	DataSectionPtr pDS = BWResource::openSection(directory + filename);
	return this->load(pDS);
}

bool ParticleSystem::load(DataSectionPtr pDS)
{
	if (!pDS)
		return false;

	DataSectionPtr versionDS = pDS->findChild("serialiseVersionData");
	if (!versionDS)
	{
		// this is in the new format, the system data is in
		pDS = pDS->openFirstSection();

		if (!pDS)
			return false;	// give up

		versionDS = pDS->findChild("serialiseVersionData");
		if (!versionDS)
			return false;	// give up
	}

	MEM_TRACE_BEGIN( directory + filename )
	bool ok = this->serialiseInternal( pDS, true, false );
	MEM_TRACE_END()

	return ok;
}


void ParticleSystem::save( std::string filename, std::string directory,
    bool transient )
{
	DataSectionPtr pDS = BWResource::openSection(directory + filename);

	if (!pDS)
		return;

	// save the system properties
	this->serialiseInternal( pDS, false, transient );

	// save cache to file
	pDS->save( directory + filename );
}

/**
 *	Serialise with a name. Serialising is saving OR loading.
 */
bool ParticleSystem::serialise( DataSectionPtr pSect, bool load,
	std::string name, bool transient )
{
	name_ = name;  	// the name is stored as an xml tag

	return serialiseInternal( pSect, load, transient );
}

/**
 *	This is the serialiser for ParticleSystem properties.
 */
bool ParticleSystem::serialiseInternal( DataSectionPtr pSect, bool load, 
    bool transient )
{
	// version number.  if any part of the particle system serialisation changes, bump this up
	const int serialiseVersionCode = 2;

	int serialiseVersionData = serialiseVersionCode;
	SERIALISE(pSect, serialiseVersionData, Int, load);
	if (serialiseVersionData != serialiseVersionCode)
	{
		ERROR_MSG( "ParticleSystem::serialiseInternal - version mismatch.  "
			"Code = %d, Data = %d.\n",
			serialiseVersionCode, serialiseVersionData );
	}

	SERIALISE(pSect, windFactor_, Float, load);
    // For backwards compatibility try and read the wind half-life.  If it's
    // read then set the windFactor to 0.5.  If it's not read then do nothing.
    if (load)
    {
        float windHL = pSect->readFloat("windHalflife_", -1.0f);
        if (windHL != -1.0f)
        {
            // Scale so that a wind half-life of 5 seconds corresponds to
            // a wind factor of 0.5.
            float k = -0.2f*logf(0.5f);
            windFactor(expf(-k*fabsf(windHL)));
        }
    }

	SERIALISE(pSect, explicitTransform_, Bool, load);
	SERIALISE(pSect, explicitPosition_, Vector3, load);
	SERIALISE(pSect, explicitDirection_, Vector3, load);
	SERIALISE(pSect, localOffset_, Vector3, load);
	SERIALISE(pSect, maxLod_, Float, load);
	SERIALISE(pSect, fixedFrameRate_, Float, load);

	// handle the actions
	const std::string ActionsString( "Actions" );	
	if (load)
	{		
		DataSectionPtr pActionSect = pSect->openSection(ActionsString);

		if (pActionSect)
		{
            actions_.clear();
            for
            (
                DataSectionIterator it = pActionSect->begin();
                it != pActionSect->end();
                ++it
            )
            {
                DataSectionPtr pDS = *it;
				std::string psaTypeStr = pDS->sectionName();
				int type = ParticleSystemAction::nameToType( psaTypeStr );
				ParticleSystemActionPtr newAction =
					ParticleSystemAction::createActionOfType( type ); 
				if (newAction)
				{
					newAction->serialise( pDS, true );
					actions_.push_back(newAction);
				}
				else
				{
					ERROR_MSG( "Unknown Particle System Action type %s\n", psaTypeStr.c_str() );
				}
            }
		}
	}
	else
	{
		DataSectionPtr pActionSect = pSect->newSection( ActionsString );

		Actions & actions = actionSet();
		Actions::iterator iter = actions.begin();
		Actions::iterator lastAction = actions.end();

		while (iter != lastAction)
		{
			(*iter)->serialise( pActionSect, false );
			iter++;
		}
	}

	// handle the renderer	
	const std::string RendererString = "Renderer";
	if (load)
	{
		DataSectionPtr pRendererSect = pSect->openSection( RendererString );
		if (pRendererSect)
		{
			// create a new one, use iterators for convenience
			DataSectionIterator it;
			for(it = pRendererSect->begin(); it != pRendererSect->end(); it++)
			{
				DataSectionPtr pDS = *it;
				std::string renderer = pDS->sectionName();

				ParticleSystemRendererPtr pr = ParticleSystemRenderer::createRendererOfType( renderer, pDS );
				MF_ASSERT(pr)

				if (pr != NULL)
				{
					pr->serialise(pDS, load);
					// have to call through the accessor,
					// so the renderer's capacity is set.
					this->pRenderer( pr.getObject() );

					// if sprite renderer, let it know if the sprites can rotate
					MF_ASSERT(pRenderer());

					if (pRenderer()->nameID() == SpriteParticleRenderer::nameID_)
					{
						bool canRotate = false;
						Actions & actions = actionSet();
						for (Actions::iterator it = actions.begin();
							it != actions.end();
							it++)
						{
							ParticleSystemActionPtr act = *it;
							if (act->typeID() == PSA_SOURCE_TYPE_ID)
							{
								SourcePSA * source = (SourcePSA*)(&*act);
								if (source->initialRotation() != Vector2::zero() ||
									source->randomInitialRotation() != Vector2::zero() )
								{
									canRotate = true;
									break;
								}
							}
						}
						SpriteParticleRenderer* sp = (SpriteParticleRenderer*)(&*pRenderer());
						sp->rotated( canRotate );
					}
				}
			}
		}
	}
	else
	{
		if (pRenderer_)
		{
			DataSectionPtr pRendererSect = pSect->newSection( RendererString );
			pRenderer_->serialise( pRendererSect, load );
		}
	}
	
	//Note : set capacity after the renderer, because the particles
	//container has probably changed and we should now sets its capacity.
	SERIALISE_TO_FUNCTION(pSect, capacity, Int, load);

	//serialise the bounding box.
	if (load)
	{
		//note bounding boxes in xml are local space.  we try and convert these to
		//world space if we can.  if we can't, somebody else will tell us to later
		//on (when we are put in the world)
		DataSectionPtr pBB = pSect->openSection( "boundingBox" );
		if (pBB)
		{
			maxBB_.setBounds( pBB->readVector3("min"), pBB->readVector3("max") );
			this->clearBoundingBox();
		}
		else
		{
			maxBB_.setBounds( Vector3(-0.01f,-0.01f,-0.01f), Vector3(0.01f,0.01f,0.01f) );
			this->clearBoundingBox();	
		}
	}
	else
	{
        if (!transient)
		    this->forceFullBoundingBoxCalculation();
		else
			maxBB_ = boundingBox_;

		DataSectionPtr pBB = pSect->openSection( "boundingBox", true );
		if (pBB)
		{
			pBB->delChildren();
			if (!boundingBox_.insideOut())
			{
				pBB->writeVector3( "min", boundingBox_.minBounds() );
				pBB->writeVector3( "max", boundingBox_.maxBounds() );
			}
			else
			{
				WARNING_MSG( "Bounding box was inside out during save\n" );
			}
		}
	}

	//Bug 5362 fix: Remove the requirement for there to be actions other than
	//	System and Renderer for a system.
	//return ( (actions_.size() > 0) && (pRenderer_ != NULL) );

	return (pRenderer_ != NULL);
}


/**
 *	This static method returns all of the resources that will be required to
 *	create the particle system.  It is designed so that the resources
 *	can be loaded in the loading thread before constructing the particle system
 */
/*static*/ void ParticleSystem::prerequisites( DataSectionPtr pSection, std::set<std::string>& output )
{
	const std::string ActionsString( "Actions" );	

	DataSectionPtr pActionsSect = pSection->openSection(ActionsString);

	if (pActionsSect)
	{        
        for
        (
            DataSectionIterator it = pActionsSect->begin();
            it != pActionsSect->end();
            ++it
        )
        {
            DataSectionPtr pActionSect = *it;
			ParticleSystemAction::prerequisitesOfType(pActionSect, output);
		}
	}	


	const std::string RendererString = "Renderer";
	
	DataSectionPtr pRendererSect = pSection->openSection( RendererString );
	if (pRendererSect)
	{
		DataSection::iterator it;
		for(it = pRendererSect->begin(); it != pRendererSect->end(); it++)
		{
			DataSectionPtr pDS = *it;
			ParticleSystemRenderer::prerequisitesOfType(pDS, output);
		}		
	}	
}


/**
 *	This method forces a bounding box calculation, by simulating
 *	the system for 10 seconds, and doing a full spawn.  It should
 *	only be used in non-time critical situations, for example during
 *	a save to xml.
 */
void ParticleSystem::forceFullBoundingBoxCalculation()
{
	forcingSave_ = true;

	BoundingBox bb( BoundingBox::s_insideOut_ );

	//calc bounding box.  spawn all our particles at once,
	//then run the simulation for 10 seconds at 30fps.
	const float timeToRun = 10.f;
	const float tickTime = 0.033f;

	//spawn particles to capacity - this means non-time-triggered systems
	//also get bb's generated.
	this->clear();
	this->spawn( this->capacity() );

	for (float ft=0.f; ft<timeToRun; ft += tickTime)
	{
		//note - call update not tick, we want to avoid the runtime bb calculation
		//mechanism (instead just run the psystem, then we calculate bb explicitly)
		this->update(tickTime);
		//need to update each frame, because particles may die in the meantime.  we
		//want the accumulated maximum.
		this->updateBoundingBox();
		if (!boundingBox_.insideOut())
		{
			bb.addBounds( boundingBox_ );
		}
	}
	// clear flares created during calculation
	if ( !flareIDs_.empty() )
		LensEffectManager::instance().killFlares( flareIDs_ );
	//bb has now accumulated the largest runtime size during 10 sec run.
	boundingBox_ = bb;
	maxBB_ = boundingBox_;

	forcingSave_ = false;
}


// -----------------------------------------------------------------------------
// Section: Accessors to Particle System Properties.
// -----------------------------------------------------------------------------

/**
 *	This is the set-accessor for the particle system's renderer. The old
 *	renderer, if any must have its reference count decremented before it is
 *	discarded by the particle system. Likewise, the renderer has its
 *	reference count incremented only if it is not NULL.
 *
 *	Note that assigning a new renderer recreates the particle container, as
 *	different renderers may have different containers.
 *
 *	@param	pNewRenderer		A pointer to the new renderer.
 */
void ParticleSystem::pRenderer( ParticleSystemRendererPtr pNewRenderer )
{
	if ( pRenderer_ != pNewRenderer )
	{
		size_t capacity = this->capacity();

		pRenderer_ = pNewRenderer;		

		if ( pRenderer_ != NULL )
		{			
			particles_ = pRenderer_->createParticleContainer();
			this->capacity( capacity );			
		}
	}
}



static Matrix s_wtMatrix;
/**
 *	This returns the worldTransform for the particle system. If the particle
 *	system is attached to a node, it uses that node's transform.
 *
 *	If there is no attached node, then the result is the identity matrix.
 *
 *	@return	The world transform matrix for the Particle System.
 */
const Matrix & ParticleSystem::worldTransform( void ) const
{
	if (explicitTransform_)
	{
		s_wtMatrix.lookAt( explicitPosition_, explicitDirection_,
			fabsf(explicitDirection_.y) < 0.9f ? Vector3(0,1,0) : Vector3(0,0,1) );
		s_wtMatrix.invert();
		s_wtMatrix.preTranslateBy(localOffset_);
		return s_wtMatrix;
	}
	else if (pOwnWorld_ != NULL)
	{
		s_wtMatrix.setTranslate(localOffset_);
		s_wtMatrix.postMultiply(pOwnWorld_->getMatrix());
		return s_wtMatrix;
	}
	else
	{
		s_wtMatrix.setTranslate(localOffset_);
		return s_wtMatrix;
	}
}

const Matrix & ParticleSystem::objectToWorld( void ) const
{
	s_wtMatrix = worldTransform();
	if ( pRenderer() != NULL )
	{
		if ( pRenderer()->local() )
		{
			s_wtMatrix = Matrix::identity;
		}
		else
		{
			Matrix scaler;
			scaler.setScale( 1.f / s_wtMatrix.applyToUnitAxisVector(0).length(), 
							 1.f / s_wtMatrix.applyToUnitAxisVector(1).length(), 
							 1.f / s_wtMatrix.applyToUnitAxisVector(2).length() );
			s_wtMatrix.preMultiply( scaler );
		}

		if ( pRenderer()->viewDependent() )
		{
			// Adjust to view, if particle system is view dependent.
			s_wtMatrix.postMultiply( Moo::rc().view() );
		}
	}
	return s_wtMatrix;
}


// -----------------------------------------------------------------------------
// Section: General Operations on Particle Systems.
// -----------------------------------------------------------------------------

/**
 *	This method adds an action to the particle system. An action may be
 *	added multiple times to the particle system to create a meta-action but
 *	it usually makes better sense to modify the attributes of an action
 *	to create the equivalent effect.
 *
 *	@param pAction		A pointer to the action to be added.
 */
void ParticleSystem::addAction( ParticleSystemActionPtr pAction )
{
	if ( pAction != NULL )
	{
		actions_.push_back( pAction );

		// If the action was a source action, pass any collision information
		// to it.
		if ( groundSpecifier() )
		{
			if ( pAction->typeID() == PSA_SOURCE_TYPE_ID )
			{
				SourcePSA *pSourcePSA = reinterpret_cast<SourcePSA *>(&*pAction);
				pSourcePSA->groundSpecifier( groundSpecifier() );
			}
		}		
	}
}

/**
 *	This method inserts an action to the particle system. An action may be
 *	added multiple times to the particle system to create a meta-action but
 *	it usually makes better sense to modify the attributes of an action
 *	to create the equivalent effect.
 *
 *	@param idx			The index at which to insert the action.
 *	@param pAction		A pointer to the action to be added.
 */
void ParticleSystem::insertAction( size_t idx, ParticleSystemActionPtr pAction )
{
	if ( pAction != NULL )
	{
		actions_.insert( actions_.begin() + idx, pAction );

		// If the action was a source action, pass any collision information
		// to it.
		if ( groundSpecifier() )
		{
			if ( pAction->typeID() == PSA_SOURCE_TYPE_ID )
			{
				SourcePSA *pSourcePSA = reinterpret_cast<SourcePSA *>(&*pAction);
				pSourcePSA->groundSpecifier( groundSpecifier() );
			}
		}		
	}
}

/**
 *	This method removes the first occurrence of an action of type actionID
 *	from the particle system. This is in order of first-created.
 *
 *	@param actionTypeID		The action type of the action to be removed.
 */
void ParticleSystem::removeAction( int actionTypeID )
{
	Actions::iterator iter = actions_.begin();
	Actions::iterator endOfVector = actions_.end();

	while ( iter != endOfVector )
	{
		ParticleSystemActionPtr pAction = *iter;
		if ( pAction->typeID() == actionTypeID )
		{
			// If the action was a source, remove any collision information
			// from it.
			if ( pAction->typeID() == PSA_SOURCE_TYPE_ID )
			{
				SourcePSA *pSourcePSA = reinterpret_cast<SourcePSA *>(&*pAction);
				pSourcePSA->groundSpecifier( NULL );
			}

			actions_.erase( iter );			
			break;
		}

		iter++;
	}
}

/**
 *	This method removes the action from the particle system.
 *
 *	@param pAction		A pointer to the action to be removed.
 */
void ParticleSystem::removeAction( ParticleSystemActionPtr pAction )
{
	if ( pAction != NULL )
	{
		Actions::iterator iter = actions_.begin();
		Actions::iterator endOfVector = actions_.end();

		while ( iter != endOfVector )
		{
			if ( *iter == pAction )
			{
				// If the action was a source, remove any collision information
				// from it.
				if ( pAction->typeID() == PSA_SOURCE_TYPE_ID )
				{
					SourcePSA *pSourcePSA = reinterpret_cast<SourcePSA *>
						(&*pAction);
					pSourcePSA->groundSpecifier( NULL );
				}

				actions_.erase( iter );				
				break;
			}

			iter++;
		}
	}
}

/**
 *	This method finds the first occurrence of an action of type actionID
 *	from the particle system. This is in order of first-created.
 *
 *	@param actionTypeID		The action type of the action to be accessed.
 */
ParticleSystemActionPtr ParticleSystem::pAction( int actionTypeID )
{
	Actions::iterator iter = actions_.begin();
	Actions::iterator endOfVector = actions_.end();

	while ( iter != endOfVector )
	{
		ParticleSystemActionPtr pAction = *iter;
		if ( pAction->typeID() == actionTypeID )
		{
			return *iter;
		}

		++iter;
	}

	return NULL;
}

/**
 *	This method finds the first occurrence of an action of type actionID
 *	from the particle system where the name is str. This is in order of 
 *  first-created.
 *
 *	@param actionTypeID		The action type of the action to be accessed.
 *  @param str              The name of the action to search for.  If this is 
 *                          empty then the first action of the appropriate id
 *                          is found.
 */
ParticleSystemActionPtr ParticleSystem::pAction
(
    int             actionTypeID, 
    std::string     const &str
)
{
    if (str.empty())
    {
        return pAction(actionTypeID);
    }
    else
    {
	    Actions::iterator iter = actions_.begin();
	    Actions::iterator endOfVector = actions_.end();

	    while ( iter != endOfVector )
	    {
		    ParticleSystemActionPtr pAction = *iter;
		    if ( pAction->typeID() == actionTypeID && pAction->name() == str)
		    {
			    return *iter;
		    }

		    ++iter;
	    }

	    return NULL;
    }
}

/**
 *	This method updates the state of the particles for the time given. There
 *	is a distinction made between the drones and non-drones. These are update
 *	slightly differently.
 *
 *	@param dTime	Time since last update in seconds.
 */
void ParticleSystem::tick( float dTime )
{
	static DogWatch tickWatch( "ParticleSystemTick" );
	ScopedDogWatch watcher( tickWatch );

	bool calcBoundingBox = false;

	// We only call the actual update on particle systems that are :
	// dynamic (meaning tick is only called by and controlled by parent)
	// static and we are told to.
	// When are static ps's told to update?
	// a) when they were visible last frame
	// b) for x seconds after loading (see ChunkParticles for implementation)
	if (!isStatic() || doUpdate_)
	{
		this->update( dTime );

		// counter is frames not calced for.
		if (++counter_ == 10)
		{
			counter_ = 0;
			calcBoundingBox = true;
		}

		// TODO : once the particle system has a good bounding box (whatever
		// that means), then we no longer need to update it every so often.	

		// OK, now actually calculate it if we ought
		if (calcBoundingBox)
		{
			updateBoundingBox();			
		}
	}
}


/**
 *	Update the particle systems for the given elapsed time
 */
void ParticleSystem::update( float dTime )
{
	dTime += framesLeftOver_;

	if ( dTime > 1.f )
		dTime = 1.f;

	while ( dTime > 0.f )
	{
		float dt = ( fixedFrameRate_ > 0.f ) ? ( 1.f / fixedFrameRate_ ) : dTime;

		//quantise the dt to be a fixed multiple of the particle's age increment.
		//this is to avoid inaccuracies in the particle's age calculation (very
		//important because that flows on to the overall lifetime of the system etc.)
		uint16 nAgeIncrements = Particle::nAgeIncrements( dt );
		if (nAgeIncrements == 0)
			break;

		dt = Particle::age( nAgeIncrements );

		if ( dt > dTime )
			if (almostEqual(dt,dTime)) // allow some inaccuracy..
				dt = dTime;
			else
				break;

		dTime -= dt;

		// Execute aging pass for each particle. The actions needs to know
		// how much time has passed so they know how to adjust the properties
		// accordingly.
		if (particles_)
		{
			Particles::iterator pIter = particles_->begin();
			Particles::iterator endOfParticles = particles_->end();
			while ( pIter != endOfParticles )
			{
				if (pIter->isAlive())
				{
					uint16 age = pIter->ageAccurate();
					if ( (uint32)age + (uint32)nAgeIncrements > (uint32)Particle::ageMax() )
						pIter->ageAccurate( Particle::ageMax() );
					else
						pIter->ageAccurate( age + nAgeIncrements );
				}
				pIter++;
			}

			// Execute each of the actions.
			Actions::iterator aIter = actions_.begin();
			Actions::iterator endOfActions = actions_.end();
			while ( aIter != endOfActions )
			{
				if ( firstUpdate_ )
					(*aIter)->setFirstUpdate();
				if ( (*aIter)->enabled() )
					(*aIter)->execute( *this, dt );
				aIter++;
			}

			firstUpdate_ = false;
		}
		
		// Pre-calculate our wind vector.
        Vector3 windEffect = 
            Math::clamp(0.0f, windFactor(), 1.0f)*windVelocity();

		// Apply movement pass for each particle. This pass applies the
		// velocities of the particle to the particle's position.
		if ( particles_ )
		{
			Particles::iterator pIter = particles_->begin();
			Particles::iterator endOfParticles = particles_->end();			
			while ( pIter != endOfParticles )
			{
				if (pIter->isAlive())
				{
					Particles::iterator particle = pIter;

					// Apply the effect of wind.
					Vector3 newVelocity = particle->getTempVelocity() + windEffect;
					particle->position() += dt * newVelocity;

				}
				pIter++;
			}
		}

		if (pRenderer_)
			pRenderer_->update( begin(), end(), dt );
	}

	framesLeftOver_ = dTime;
	doUpdate_ = false;
}


/**
 *	This methods tells the particle system to draw itself.
 *
 *	Particles are stored in world coordinates so we do not need
 *	the attachment transform which is passed in.
 */
void ParticleSystem::draw( const Matrix &world, float lod )
{
	// make sure particle systems have not been globally disabled,
	// and that we are close enough to be drawn
	//PCWJ TODO
	//float maxLod = LodSettings::instance().applyLodBias(maxLod_);
	float maxLod = maxLod_;
	if (!ParticleSystem::active() || (maxLod > 0.f && lod > maxLod))
		return;	

	// and now get the renderer to draw us
	if (pRenderer_ != NULL)
	{
		pRenderer_->draw( world, this->begin(), this->end(), this->boundingBox() );
	}

	// tick for at least the next frame.  this also means that visible
	// particle systems get ticked at all.
	doUpdate_ = true;	
}


/**
 *	This accumulates our bounding box into the given matrix
 */
void ParticleSystem::boundingBoxAcc( BoundingBox & bb, bool skinny )
{
	if (!isStatic() && particles_ && !particles_->size())
		return;

	if( boundingBox_.insideOut() )
		return;

	if (!this->isLocal())
	{
		// unfortunately they want the bb in local coords!
		Matrix fastInvWorld;
		const Matrix & world = worldTransform();
		const Vector3 & worldPos = world.applyToOrigin();
		fastInvWorld.setRotateY( -world.yaw() );
		BoundingBox tbb(	boundingBox_.minBounds() - worldPos,
							boundingBox_.maxBounds() - worldPos );
		tbb.transformBy( fastInvWorld );
		bb.addBounds( tbb );
	}
	else
	{
		bb.addBounds( boundingBox_ );
	}
}

void ParticleSystem::visibilityBoxAcc( BoundingBox & vbb )
{
	if (!isStatic() && particles_ && !particles_->size())
		return;

	if( maxBB_.insideOut() )
		return;

	vbb.addBounds( maxBB_ );
}

/**
 *	We have been attached. Give ourselves a ground specifier
 */
bool ParticleSystem::attach( MatrixLiaison * pOwnWorld )
{
	if (attached_) return false;

	attached_ = true;
	pOwnWorld_ = pOwnWorld;	

	this->groundSpecifier( new ChunkRompCollider() );
	//before the particle system is put in the world, it has a local space
	//bounding box - so here we transform it into world space.
	this->clearBoundingBox();
	return true;
}


void ParticleSystem::detach()
{
	attached_ = false;
	pOwnWorld_ = NULL;
}


void ParticleSystem::tossed(bool outside)
{
}


void ParticleSystem::move(float dTime)
{
}


/**
 *	Make sure u convert back to world coordinates when entering world.
 */
void ParticleSystem::enterWorld()
{
	this->clearBoundingBox();
}

/**
 *	Make sure to convert back to local coordinates when leaving world.
 */
void ParticleSystem::leaveWorld()
{
	if (!this->boundingBox_.insideOut())
	{	
		Matrix worldToLocal = worldTransform();
		worldToLocal.invert();
		this->boundingBox_.transformBy( worldToLocal );
	}
}


/**
 *	This method tells the particle system to remove all particles from itself.
 */
void ParticleSystem::clear( void )
{
	if ( !flareIDs_.empty() )
		LensEffectManager::instance().killFlares( flareIDs_ );
	if ( particles_ )
		particles_->clear();	
	this->clearBoundingBox();	
}


/**
 *	This method sets a new 'empty' bounding box, which for psystems is
 *	a small (20cm) box positioned in world space.  This is necessary because
 *	if psystems aren't seen, they aren't calculated (and thus won't ever get
 *	any particles).
 */
void ParticleSystem::clearBoundingBox()
{
	boundingBox_.setBounds( Vector3(-0.01f,-0.01f,-0.01f), Vector3(0.01f,0.01f,0.01f) );
	if (!this->isLocal())
	{
		this->boundingBox_.transformBy( pOwnWorld_->getMatrix() );
	}
}


/**
 *	This is the Set-Accessor for the ground specifier of the particle
 *	system.
 *
 *	@param pGS	A smart pointer to the new ground specifier.
 */
void ParticleSystem::groundSpecifier( RompColliderPtr pGS )
{
	pGS_ = pGS;

	// Need to inform all source actions of the ground specifier.
	if ( groundSpecifier() )
	{
		Actions::iterator iter = actions_.begin();
		Actions::iterator endOfVector = actions_.end();

		while ( iter != endOfVector )
		{
			ParticleSystemActionPtr pAction = *iter;
			if ( pAction->typeID() == PSA_SOURCE_TYPE_ID )
			{
				SourcePSA *pSourcePSA = reinterpret_cast<SourcePSA *>(&*pAction);
				pSourcePSA->groundSpecifier( groundSpecifier() );
			}

			++iter;
		}
	}
}


size_t ParticleSystem::sizeInBytes() const
{
	size_t footprint = sizeof(ParticleSystem);
	if (particles_)
		footprint += particles_->capacity() * Particle::sizeInBytes();
	if (pRenderer_)
		footprint += pRenderer_->sizeInBytes();

	Actions::const_iterator iter = actions_.begin();
	Actions::const_iterator endOfVector = actions_.end();

	while ( iter != endOfVector )
	{
		ParticleSystemActionPtr pAction = *iter;
		footprint += pAction->sizeInBytes();
		++iter;
	}

	return footprint;
}


/**
 *	This method spawns a number of particles on-demand.
 *
 *	@param	num		The number of particles to spawn.
 */
void ParticleSystem::spawn( int num )
{
	for (ParticleSystem::Actions::iterator actionIt = actions_.begin();
			actionIt != actions_.end();
			actionIt++)
	{
		if ((*actionIt)->typeID() == PSA_SOURCE_TYPE_ID)
		{
			SourcePSA * source = (SourcePSA *)((*actionIt).getObject());
			source->force(num);
		}
	}
}


/**
 *	This is the Set-Accessor for the capacity property.
 *
 *	@param number	The new maximum number of particles allowed.
 */
void ParticleSystem::capacity( int number )
{
	//Code error - particle system storage should be constructed before
	//setting the capacity.  Usually this means having a renderer first.
	MF_ASSERT( particles_.getObject() )	

	if (number > MAX_CAPACITY)
	{
		number = MAX_CAPACITY;
		WARNING_MSG( "ParticleSystem::capacity : Maximum Capacity exceeded - setting capacity to %d\n", MAX_CAPACITY );
	}

	{
		memoryCounterSub( particles );
		memoryClaim( particles_ );
	}

	int currentCapacity = particles_->capacity();

	if (number != currentCapacity)
	{
		this->clear();
		particles_->reserve( number );
	}

	{
		memoryCounterAdd( particles );
		memoryClaim( particles_ );
	}

	if ( pRenderer_ )
		pRenderer_->capacity( number );
}


float ParticleSystem::duration() const
{
	float duration = -1;

	Actions::const_iterator it = actions_.begin();

	while ( it != actions_.end() )
	{
		if ( (*it)->typeID() == PSA_SINK_TYPE_ID )
		{
			const SinkPSA *pSinkPSA = reinterpret_cast<const SinkPSA *>((*it).getObject());
			duration = max(pSinkPSA->maximumAge(), duration);
		}

		++it;
	}

	return duration;
}


void ParticleSystem::updateBoundingBox()
{
	this->clearBoundingBox();

    float sz = 0.01f; // Potential size of the largest particle

    // Iterate through the particles, expanding the bounding box by 
    // the positions.  In addition, get the size of the largest particle
    // if not a mesh render.
	bool meshRenderer = false;
	if (pRenderer())
	{
		meshRenderer = pRenderer()->isMeshStyle();
	}

	Particles::iterator iter = this->begin();
	while (iter != this->end())
	{
		Particle & p = *iter;
		if (p.isAlive())
		{	
			boundingBox_.addBounds( p.position() );
			if (!meshRenderer)
				sz = std::max(sz, p.size());
		}
		++iter;
	}

	// Query the renderer about further expansion of the bounding box.
	if (pRenderer() && pRenderer()->knowParticleRadius())
		sz = std::max(sz, pRenderer()->particleRadius());

	// Expand the bounding box by the potential size of the largest
	// particle.
	boundingBox_.expandSymmetrically(sz, sz, sz);	
	
	// update so you know the maximum bounding box
	if (!this->isLocal())
	{
		// unfortunately they want the bb in local coords!
		Matrix fastInvWorld;
		const Matrix & world = worldTransform();
		const Vector3 & worldPos = world.applyToOrigin();
		fastInvWorld.setRotateY( -world.yaw() );
		BoundingBox tbb(	boundingBox_.minBounds() - worldPos,
							boundingBox_.maxBounds() - worldPos );
		tbb.transformBy( fastInvWorld );
		maxBB_.addBounds( tbb );
	}
	else
	{
		maxBB_.addBounds( boundingBox_ );
	}	
}


/**
 *	This method is called to set a flag on the particle system indicating
 *	it is static or not.  In reality this method only gets called with true.
 *	If true, then it means the particle system has been placed as a chunk
 *	item.
 *
 *	@param	s	static or not.
 */
void ParticleSystem::isStatic( bool s )
{
	static_ = s;
}

/**
 *	This is the hash function which gets a uniqueID for the particle
 *
 *	@param p	The particle to get the uniqueID for
 *	@param ps	The particle system that the particle belongs to
 */
/*static*/ uint32 ParticleSystem::getUniqueParticleID( const Particle& p, const ParticleSystem& ps )
{
	return p.index() + ps.id() * MAX_CAPACITY + MAX_CAPACITY;
}


// particle_system.cpp
