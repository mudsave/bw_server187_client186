/*
** 此模块用于扩展BigWorld模块的功能，改写一些能明显带来性能改善的脚本函数
*/
#include <stdlib.h>
#include "cellapp/entity.hpp"
#include "cellapp/entity_navigate.hpp"
#include "cellapp/cellapp.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "pyscript/script_math.hpp"
#include "../csdefine.h"
#include "../entity_extras/csolExtra.hpp"
#include "../entity_extras/monster.hpp"

//以下定义在cellextra/gameobject.hpp中已经声明，在gameobject.cpp
//中也有定义，这里不能进行重复声明和定义
//PY_SCRIPT_CONVERTERS_DECLARE( Entity )
//PY_SCRIPT_CONVERTERS( Entity )

/**
 * 定义Entity存放列表，用于搜索指定范围的Entity时存放Entity
 * 使用未命名名字空间进行限定，使这些定义只在本文档中可见
*/
namespace{
	const float MIN_ATTACK_RADIUS = 1.2;		//最小攻击半径
	const float DEFAULT_MAX_MOVE = 4.0;			//默认最大散开移动距离

	typedef std::vector<Entity *> EntityList;

	class ExtraEntityReceiver: public Entity::EntityReceiver
	{
	public:
		~ExtraEntityReceiver() { entities_.clear(); }
		void addEntity( Entity * pEntity )
		{
			entities_.push_back( pEntity );
		}
		EntityList::iterator begin() { return entities_.begin(); }
		EntityList::iterator end() { return entities_.end(); }
		void clear() { entities_.clear(); }
		const EntityList & entities() const { return entities_; }
	private:
		EntityList entities_;
	};

	bool getMonsterExtra( Monster ** pMon, Entity * pEnt )
	{
		//GameObject * gobj = CsolExtra::instance(*pEnt).getMapInstancePtr();
		//*pMon = dynamic_cast<Monster *>(gobj);
		*pMon = CsolExtra::extraProxy<Monster *>( pEnt );
		return *pMon != NULL;
	}
}

/**************************************************************************/
/**
* xz平面上中心点距离，两个entity中心点之间的距离
*
* 源自：cell/utils.py ent_flatDistance
*/
inline float ent_flatDistance( Entity * pSrc, Entity * pDst )
{
	float x = pSrc->position().x - pDst->position().x;
	float z = pSrc->position().z - pDst->position().z;
	return sqrt( x*x + z*z );
}

/**
* entity的boundingBox z 方向长度的一半
*
* 源自：cell/utils.py ent_length
*/
inline float ent_hLength( Entity * pEntity )
{
	Monster * pEntExtra = NULL;
	if( !getMonsterExtra(&pEntExtra, pEntity) )
		return 0;
	return pEntExtra->BoundingBoxHZ();
}

/**
* entity的boundingBox x 方向长度的一半
*
* 源自：cell/utils.py ent_width
*/
inline float ent_hWidth( Entity * pEntity )
{
	Monster * pEntExtra = NULL;
	if( !getMonsterExtra(&pEntExtra, pEntity) )
		return 0;
	return pEntExtra->BoundingBoxHX();
}

/**
* z轴方向，两个entity面对面紧贴着站的距离
*
* 源自：cell/utils.py ent_z_distance_min
*/
inline float ent_z_distance_min( Entity * pSrc, Entity * pDst )
{
	return ent_hLength(pSrc) + ent_hLength(pDst);
}

/**
* z轴方向的距离，两个entity面对面方向的距离，不包含boundingBox
*
* 源自：cell/utils.py ent_z_distance
*/
inline float ent_z_distance( Entity * pSrc, Entity * pDst )
{
	return ent_flatDistance(pSrc, pDst) - ent_z_distance_min(pSrc, pDst);
}

/**
* x轴方向，两个entity平排紧贴着站的距离
*
* 源自：cell/utils.py ent_x_distance_min
*/
inline float ent_x_distance_min( Entity * pSrc, Entity * pDst )
{
	return ent_hWidth(pSrc) + ent_hWidth(pDst);
}

/**
* x轴方向的距离，两个entity平排站的距离，不包含boundingBox
*
* 源自：cell/utils.py ent_x_distance
*/
inline float ent_x_distance( Entity * pSrc, Entity * pDst )
{
	return ent_flatDistance(pSrc, pDst) - ent_x_distance_min(pSrc, pDst);
}


/**************************************************************************/
/**
* 检测是否能重新定位自己的位置
* pEnt		: 需要进行定位的entity
*
* 源自：cell/utils.py checkForLocation
*/
bool checkForLocation_cpp( Entity * pEnt )
{
	Monster * pEntExtra = NULL;
	if( !getMonsterExtra(&pEntExtra, pEnt) )
		return false;

	if( pEntExtra->actionSign(ACTION_FORBID_MOVE) )
	{
		DEBUG_MSG( "I cannot move.\n" );
		return false;
	}

	int counter = pEntExtra->queryTemp( "disperse_counter", 0 );
	pEntExtra->setTemp( "disperse_counter", counter + 1 );
	// 沿用旧规则，为零不进行重定位；每4次最多进行一次分散检测
	return counter > 0 && counter % 4 == 0;
}

/**
* 在xz平面的center点为圆心，半径是radius的圆上，随机取弧度范围是radian，
	朝向是yaw的点
* center	: Position3D, 圆心位置
* radius	: float，半径
* radian	: float，0 - 2*pi，单位是弧度，表示取多少圆弧度
* yaw		: float，0 - 2*pi，单位是弧度，表示偏转多少弧度，正值逆时针，负值顺时针
*
* 源自：cell/utils.py randomPosAround
*/
Position3D randomPosAround( const Position3D & center, float radius, float radian, float yaw )
{
	float minRadian, randomRadian, x, z;

	minRadian = 2*M_PI - radian*0.5 + yaw;
	//maxRadian = 2*M_PI + radian*0.5 + yaw;
	//randomRadian = minRadian + (maxRadian - minRadian) * unitRand();	// 这条公式简化为下面这条公式
	randomRadian = minRadian + radian * unitRand();

	x = center.x + radius * cos(randomRadian);
	z = center.z + radius * sin(randomRadian);
	return Position3D(x, center.y, z);
}

/**
* 在某个中心点周围指定范围内散开。maxMove的作用是限制每次移动的
	最大距离，最初始的期望是用于防止怪物移动时穿过目标，移动到目标
	背后，而实际测试表明，maxMove限制得太小时，总是很难找到符合全
	部条件的点，于是即使最后获取的点不符合maxMove条件，只要符合
	canNavigateTo条件依然将此点作为目标点。
* pEnt		: 需要进行散开的entity
* centerPos	: 散开的中心点，在该中心点指定的半径上散开
* radius	: 散开的半径
* maxMove	: 最大移动距离
*
* 源自：cell/utils.py disperse
*/
bool disperse_cpp( Entity * pEnt, const Position3D & centerPos, float radius, float maxMove )
{
	Position3D dstPos, collideSrcPos, collideDstPos;
	PyObject *pyDstPos;
	float collideDist;
	float yaw = -(centerPos - pEnt->position()).yaw() - M_PI/2.0;
	float dispersingRadian = asin(std::min(maxMove, 2*radius)/radius*0.5)*4;
	bool dstFound = false;
	int tryCounter = 5;			//尝试次数

	while( tryCounter-- > 0 )
	{
		dstPos = randomPosAround( centerPos, radius, dispersingRadian, yaw );

		//上下取两个点,碰撞检测
		collideSrcPos.set( dstPos.x, dstPos.y + 10.0f, dstPos.z );
		collideDstPos.set( dstPos.x, dstPos.y - 10.0f, dstPos.z );
		collideDist = pEnt->pChunkSpace()->collide( collideSrcPos, collideDstPos, ClosestObstacle::s_default );
		if( collideDist < 0.f )
			continue;

		//由于知道两个碰撞点是在目标位置上进行上下偏移取的点，所以简化目标点计算
		dstPos.set( collideSrcPos.x, collideSrcPos.y - collideDist, collideSrcPos.z );

		//寻路检测
		pyDstPos = EntityNavigate::instance( *pEnt ).canNavigateTo( dstPos );
		if( pyDstPos == Py_None )
		{
			dstFound = false;
			Py_XDECREF( pyDstPos );
			continue;
		}
		else
		{
			dstFound = true;
			Script::setData( pyDstPos, dstPos, "EntityNavigate::instance().canNavigateTo" );
			Py_XDECREF( pyDstPos );
			break;
		}
	}

	if( !dstFound )
	{
		DEBUG_MSG( "BigWorld module extra::disperse_cpp, destination not found.\n" );
		return false;
	}
	else
	{
		PyObject *goPosResult = PyObject_CallMethod( ( PyObject * )pEnt, "gotoPosition", "O", Script::getData( dstPos ) );
		Py_XDECREF( goPosResult );
		return true;
	}
}

/**
* 定位entity的位置
* pEnt		: 需要进行定位的entity
* maxMove	: 最大移动距离
*
* 源自：cell/utils.py locate
*/
bool locate_cpp( Entity * pEnt, float maxMove )
{
	Monster * pEntExtra = NULL;
	if( !getMonsterExtra(&pEntExtra, pEnt) )
		return false;

	// 是否存在不散开标记
	if( pEntExtra->hasFlag(ENTITY_FLAG_NOT_CHECK_AND_MOVE) )
		return false;

	// 是否正在移动中，可能是因为散开时，entity速度较小，所以移动得比较慢
	if( pEntExtra->isMoving() )
		return true;

	//是否能进行散开
	if( !checkForLocation_cpp(pEnt) )
		return false;

	//目标检查
	Entity * pTarget = CellApp::pInstance()->findEntity(pEntExtra->targetID());
	if( pTarget == NULL )
		return false;

	//检查跟目标的boundingBox是否重叠
	bool needDisperse = ent_z_distance(pEnt, pTarget) < 0;
	//如果boundingBox不重叠，则看看身边是否有其他entity
	if( !needDisperse )
	{
		static ExtraEntityReceiver entReceiver = ExtraEntityReceiver();
		entReceiver.clear();
		pEnt->getEntitiesInRange( entReceiver, pEntExtra->BoundingBoxHX() + 5 );
		//needDisperse = entReceiver.entities().size() > 0;
		GameObject * pEntAround;
		for( EntityList::iterator it = entReceiver.begin(); it != entReceiver.end(); it++ )
		{
			pEntAround = CsolExtra::extraProxy<GameObject *>( *it );
			if( pEntAround != NULL && pEntAround->utype() != ENTITY_TYPE_SPAWN_POINT && \
				ent_x_distance( pEnt, *it ) < 0 )
			{
				needDisperse = true;
				break;
			}
		}
	}

	if( needDisperse )
	{
		float radius = std::max(MIN_ATTACK_RADIUS, std::max(ent_z_distance_min(pEnt, pTarget), ent_flatDistance(pEnt, pTarget)));
		return disperse_cpp(pEnt, pTarget->position(), radius, maxMove );
	}
	else
	{
		return false;
	}
}

/**
* 检查自身一定范围内的是否有没有处于移动状态的entity，有则随机移动到一个位置；
* 此功能用于解决一堆怪物追击同一个目标时重叠在一起的问题。
* 相关参数可以搜索"firstAttackAfterChase"；
*
* 源自：cell/utils.py checkAndMove
*/
bool checkAndMove_csol( Entity * pEnt )
{
	Monster * pEntExtra = NULL;
	if( !getMonsterExtra(&pEntExtra, pEnt) )
		return false;

	//最大移动距离取boudingBox宽度一半加上默认最大移动距离
	return locate_cpp( pEnt, ent_hWidth(pEnt) + DEFAULT_MAX_MOVE );
}
PY_AUTO_MODULE_FUNCTION( RETDATA, checkAndMove_csol, ARG( Entity *, END ), BigWorld )

/**
* 和 checkAndMove 一样的功能，可传入最大移动距离参数
*
* 源自：cell/utils.py checkAndMoveByDis
*/
bool checkAndMoveByDis_csol( Entity * pEnt, float maxMove )
{
	return locate_cpp( pEnt, maxMove );
}
PY_AUTO_MODULE_FUNCTION( RETDATA, checkAndMoveByDis_csol, ARG(Entity *, ARG(float, END )), BigWorld )

/**
* 让一个entity在目标entity周围指定半径圆周上散开
*
* 源自：cell/utils.py moveOut
*/
bool moveOut_csol( Entity * pSrcEntity, Entity * pDstEntity, float distance, float moveMax )
{
	Monster * pSrcEntExtra = NULL;
	if( !getMonsterExtra(&pSrcEntExtra, pSrcEntity) )
		return false;

	// 如果当前正在移动中，则不执行散开
	if( pSrcEntExtra->isMoving() )
		return true;

	//散开半径需要加上boudingBox的距离
	float radius = distance + ent_z_distance_min(pSrcEntity, pDstEntity);
	return disperse_cpp( pSrcEntity, pDstEntity->position(), radius, ent_hWidth(pSrcEntity) + moveMax );
}
PY_AUTO_MODULE_FUNCTION( RETDATA, moveOut_csol, ARG(Entity *, ARG(Entity *, ARG(float, ARG(float, END )))), BigWorld )
