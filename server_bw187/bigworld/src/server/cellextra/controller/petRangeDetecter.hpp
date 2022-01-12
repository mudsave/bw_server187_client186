/*
** 每一个tick检测role和pet的距离，进入或离开指定距离时回调脚本
*/

#ifndef CSOL_CELL_CONTROLLER_PET_DETECTER
#define CSOL_CELL_CONTROLLER_PET_DETECTER



#include "cellapp/controller.hpp"
#include "cellapp/updatable.hpp"

typedef SmartPointer<Entity> EntityPtr;


class PetDistanceController : public Controller, public Updatable
{
	DECLARE_CONTROLLER_TYPE( PetDistanceController )
public:
	PetDistanceController( int32 petID = 0, int32 distance = 0 );

	virtual void	startReal( bool isInitialStart );
	virtual void	stopReal( bool isFinalStop );

	void	writeRealToStream( BinaryOStream& stream );
	bool	readRealFromStream( BinaryIStream& stream );
	void	update();

private:
	int32 petID_;
	int32 range_;		// 设定的通知距离
	int32 oldDistance_;	// 上一个tick双方的距离
	
};


#endif

