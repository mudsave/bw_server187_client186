#include "petRangeDetecter.hpp"
#include "cellapp/cellapp.hpp"
#include "cellapp/entity.hpp"

DECLARE_DEBUG_COMPONENT(0)


IMPLEMENT_EXCLUSIVE_CONTROLLER_TYPE(
		PetDistanceController, DOMAIN_REAL, "Movement" )

PetDistanceController::PetDistanceController( int32 petID, int32 distance ):
	petID_( petID ), range_( distance )
{}


void PetDistanceController::update()
{
	Entity *ptrPet = CellApp::pInstance()->findEntity( petID_ );
	float dis = ( entity().position() - ptrPet->position() ).length();

	if( oldDistance_ > range_ )
	{
		if( dis < range_ )
		{
			this->standardCallback("onPetEnterRange");
		}
	}
	else
	{
		if( dis > range_ )
		{
			this->standardCallback("onPetLeaveRange");
		}
	}
	oldDistance_ = dis;
}

/**
 *	This method overrides the Controller method.
 */
void PetDistanceController::startReal( bool /*isInitialStart*/ )
{
	MF_ASSERT( entity().isReal() );
	CellApp::instance().registerForUpdate( this );
}


/**
 *	This method overrides the Controller method.
 */
void PetDistanceController::stopReal( bool /*isFinalStop*/ )
{
	MF_VERIFY( CellApp::instance().deregisterForUpdate( this ) );
}

void PetDistanceController::writeRealToStream( BinaryOStream& stream )
{
	this->Controller::writeRealToStream( stream );
	stream << petID_ << range_;
}

bool PetDistanceController::readRealFromStream( BinaryIStream& stream )
{
	bool result = this->Controller::readRealFromStream( stream );
	stream >> petID_ >> range_;

	return result;
}


