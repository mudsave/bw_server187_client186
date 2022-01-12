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

#include "lens_effect_manager.hpp"
#include "moo/render_context.hpp"
#include "cstdmf/debug.hpp"
#include "photon_occluder.hpp"
#include "resmgr/bwresource.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 2 )

#ifndef CODE_INLINE
#include "lens_effect_manager.ipp"
#endif

///static initialiser
uint32	LensEffectManager::s_drawCounter_ = 0;


///constructor
LensEffectManager::LensEffectManager()
{
	lastFlareVisibleCheckSource_ = Vector4(0,0,0,0);
}


///destructor
LensEffectManager::~LensEffectManager()
{
}


///this method releases all of the materials.
///it should be called before the cleanup of static
///objects ( like the lens effect manager singleton )
///because there is an order dependency on ManagedEffects.
void LensEffectManager::finz()
{
	materials_.clear();
}


/**
 *	This method returns the static LensEffectManager instance
 */
LensEffectManager & LensEffectManager::instance()
{
	static LensEffectManager instance_;

	return instance_;
}


/**
 *	This method draws all current lens effects.
 */
void LensEffectManager::draw()
{
	//Setup rendering state for lens flares

	Moo::rc().setVertexShader( NULL );
	Moo::rc().setFVF( Moo::VertexTLUV::fvf() );

	Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );

	//Draw things
	LensEffects::iterator lit = lensEffects_.begin();
	LensEffects::iterator lend = lensEffects_.end();

	while( lit != lend )
	{
		LensEffect & l = *lit++;

		l.draw();
	}

	//Restore rendering state
	Moo::rc().setRenderState( D3DRS_LIGHTING, TRUE );
	Moo::rc().setRenderState( D3DRS_CLIPPING, FALSE );

	//Increment draw counter
	s_drawCounter_++;
}


/**
 *	This method updates all current lens effects.
 *
 *	@param dTime the change in time since the last frame
 */
void LensEffectManager::tick( float dTime )
{
	dTime_ = dTime;

	//Start occlusion testing
	PhotonOccluders::iterator pit = photonOccluders_.begin();
	PhotonOccluders::iterator pend = photonOccluders_.end();

	while( pit != pend )
	{
		(*pit++)->beginOcclusionTests();
	}

	//Tick things
	LensEffects::iterator it = lensEffects_.begin();
	LensEffects::iterator end = lensEffects_.end();

	lastFlareVisibleCheckSource_ = Vector4( 0, 0, 0, -100 );

	uint64 startTime = timestamp();
	uint64 maxExecutionTime = stampsPerSecond() / 1000;

	while( it != end )
	{
		LensEffect & l = *it++;

		//collide with scene.
		//results :
		//if ( ( timestamp() - startTime < maxExecutionTime ) && ( flareVisible( l ) ) )
		if ( flareVisible( l ) )
		{
			l.tick( -dTime_ );
		}
		else
		{
			l.tick( dTime_ );
		}
	}

	//End occlusion testing
	pit = photonOccluders_.begin();
	pend = photonOccluders_.end();

	while( pit != pend )
	{
		(*pit++)->endOcclusionTests();
	}

	this->killOld();
}


/**
 *	This method performs visibility analysis on a single
 *	lens effect.  All of the plug-in photon occluders
 *	are checked against.
 *
 *	@param l	the lensEffect to check.
 *	@return true if the lens flare is visible
 */
bool LensEffectManager::flareVisible( LensEffect & l )
{
	bool offScreen = false;

	//First, check if the lens effect is too far away
	float dist = Vector3( l.position()- Moo::rc().invView().applyToOrigin() ).lengthSquared();
	if ( dist > l.maxDistance() * l.maxDistance() )
		offScreen = true;

	if ( !offScreen )
	{
		//Then, check the lens effect projects onto the screen
		Vector4 projPos;
		Vector4 in( l.position().x, l.position().y, l.position().z, 1.f );
		Moo::rc().viewProjection().applyPoint( projPos, in );

		if ( projPos.x < -projPos.w )
			offScreen = true;
		if ( projPos.x > projPos.w )
			offScreen = true;
		if ( projPos.y < -projPos.w )
			offScreen = true;
		if ( projPos.y > projPos.w )
			offScreen = true;
		if ( projPos.w <= 0.f )
			offScreen = true;

		//Check it was added this frame
		offScreen |= (l.added() != s_drawCounter_);		

		bool split = false;

		if ( !offScreen )
		{
			if (in == lastFlareVisibleCheckSource_)
			{
				offScreen = lastFlareVisibleCheckResults_[0];
				split = lastFlareVisibleCheckResults_[1];
			}
			else
			{
				Vector3 cameraPosition = Moo::rc().invView().applyToOrigin();

				//Move flare position towards camera by 50cm., to account for
				//geometry the lens flare may be residing within.
				Vector3 dir( l.position() - cameraPosition  );
				dir.normalise();
				dir *= 0.5f;
				Vector3 testPos = l.position() - dir;

				//Check our plug-in photon occluders
				cameraPosition += Moo::rc().invView().applyToUnitAxisVector( 2 ) *
					( Moo::rc().camera().nearPlane() * 1.01f );

				PhotonOccluders::iterator it = photonOccluders_.begin();
				PhotonOccluders::iterator end = photonOccluders_.end();

				while( it != end  && !offScreen )
				{
					PhotonOccluder * occluder = *it++;

					PhotonOccluder::Result result = occluder->collides( testPos, cameraPosition, l );

					switch ( result )
					{
					case PhotonOccluder::SOLID:
						//no need to continue testing
						offScreen = true;
						break;
					case PhotonOccluder::DAPPLED:
						split = true;
						break;
					case PhotonOccluder::NONE:	//fall through
					default:
						break;
					}
				}

				lastFlareVisibleCheckSource_ = in;
				lastFlareVisibleCheckResults_[0] = offScreen;
				lastFlareVisibleCheckResults_[1] = split;
			}
		}

		if  ( l.isSunFlare() || l.isSunSplit() )
		{
			if ( split )
			{
				if ( l.isSunFlare() )
					offScreen = true;
			}
			else
			{
				if ( l.isSunSplit() )
					offScreen = true;
			}
		}
		else if ( split )
		{
			offScreen = true;
		}
	}

	return ( !offScreen );
}


/**
 *	This method adds a lens effect for a single frame to the manager.
 *	If the lens effect is already in existence( if the id matches
 *	one already in the list ), then it is simply refreshed.
 *
 *	@param	id		a unique id for the lens flare.  This can be anything
 *					that is persistent between frames.
 *	@param	worldPosition the world position of the lens effect
 *	@param	le		contains the details about the lens effect to add.
 */
void LensEffectManager::add( uint32 id,
					const Vector3 & worldPosition,
					const LensEffect & le )
{
	//See if this lens effect is already there
	LensEffects::iterator it = lensEffects_.begin();
	LensEffects::iterator end = lensEffects_.end();

	while( it != end )
	{
		LensEffect & l = *it++;

		if ( l.id() == id )
		{
			l.position( worldPosition );
			l.colour( le.colour() );
			l.added( s_drawCounter_ );

			//if flare is in it's "grace period" of 1 sec, but now it is
			//being added again, then give it a valid age.
			//i.e. don't allow it a grace period of 1 sec before being drawn again.

			if (l.age() > OLDEST_LENS_EFFECT)
				l.age( OLDEST_LENS_EFFECT );

			return;
		}
	}


	//This is a new lens effect.  add to our list
	LensEffect newEffect( le );
	newEffect.id( id );
	newEffect.position( worldPosition );
	newEffect.age( OLDEST_LENS_EFFECT );
	newEffect.added( s_drawCounter_ );

	lensEffects_.push_back( newEffect );
}


/**
 *	This method causes the lens effect manager to forget about the
 *	given lens effect id. It will accept no more updates for the existing
 *	lens effect with that id. This is only necessary if you want to reuse
 *	the id for a different lens effect.
 */
void LensEffectManager::forget( uint32 id )
{
	LensEffects::iterator it = lensEffects_.begin();
	LensEffects::iterator end = lensEffects_.end();

	while( it != end )
	{
		LensEffect & l = *it++;

		if ( l.id() == id )
		{
			l.id( 0 );
			return;
		}
	}
}

/**
 *	This method kills all lens effects from the list, with 
 * 	exception of the sun.
 */
void LensEffectManager::clear()
{
	LensEffects::iterator it = lensEffects_.begin();

	while( it != lensEffects_.end() )
	{
		LensEffect & l = *it;

		if ( l.isSunFlare() || l.isSunSplit() )
		{		
			it++;
		}
		else
		{
			it = lensEffects_.erase( it );
		}
	}
}

/**
 *	This method culls lens effects from the list corresponding to the supplied
 *	id list.
 *
 *	@param	ids the id's of the lens effects to cull from the list.
 */
void LensEffectManager::killFlares( const std::set<uint32> & ids )
{
	for ( std::set<uint32>::const_iterator it = ids.begin(); it != ids.end(); it++ )
	{
		LensEffects::iterator lt = lensEffects_.begin();

		while( lt != lensEffects_.end() )
		{
			LensEffect & l = *lt;

			if ( l.id() == (*it) )
			{		
				lt = lensEffects_.erase( lt );
			}
			else
			{
				lt++;
			}
		}
	}
}

/**
 *	This method culls all dead lens effects from the list.
 */
void LensEffectManager::killOld()
{
	//Cull things
	LensEffects::iterator it = lensEffects_.begin();

	while( it != lensEffects_.end() )
	{
		LensEffect & l = *it;

		if ( l.age() > OLDEST_LENS_EFFECT + 1.f )
		{		// give 1s grace to avoid thrashing invisible effects
			it = lensEffects_.erase( it );
		}
		else
		{
			it++;
		}
	}
}


/**
 *	This method sets the appropriate material for the
 *	given lens effect
 *
 *	@param le the lens effect
 */
Moo::EffectMaterial* LensEffectManager::getMaterial( const LensEffect & le )
{
	if (le.material() != "" )
	{
		Materials::iterator it = materials_.get( le.material() );
		return it->second;
	}

	return NULL;
}


/**
 *	This method preloads the material used by this lens effect
 */
void LensEffectManager::preload( const LensEffect & le )
{
	if (le.material() != "")
		materials_.get( le.material() );
}

/**
 *	Method to remove a photon occluder
 */
void LensEffectManager::delPhotonOccluder( PhotonOccluder * occluder )
{
	PhotonOccluders::iterator it = std::find(
		photonOccluders_.begin(), photonOccluders_.end(), occluder );
	if (it != photonOccluders_.end()) photonOccluders_.erase( it );
}


/**
 *	This method deletes this vector properly
 */
LensEffectManager::Materials::~Materials()
{
	this->clear();
}


void LensEffectManager::Materials::clear()
{
	while (!this->empty())
	{
		delete (this->begin())->second;
		this->erase(this->begin());
	}
}


/**
 *	This method gets the named material from this material map,
 *	creating it if it does not exist. If the material can't be found,
 *	then a blank (unfogged) amterial is used instead.
 */
LensEffectManager::Materials::iterator LensEffectManager::Materials::get(
	const std::string & resourceID )
{
	iterator it = this->find( resourceID );
	if (it == this->end())
	{
		value_type v( resourceID, new Moo::EffectMaterial() );
		DataSectionPtr pSection = BWResource::openSection(resourceID);
		if (pSection.getObject()!=NULL)
		{
			v.second->load( pSection );
			it = this->insert( v ).first;
		}
		else
		{
			CRITICAL_MSG( "Could not load %s\n", resourceID.c_str() );
			return this->end();
		}
	}
	return it;
}



/*lens_effect_manager.cpp*/
