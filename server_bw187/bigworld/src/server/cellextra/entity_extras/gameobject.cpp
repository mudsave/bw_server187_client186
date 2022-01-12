#include "gameobject.hpp"
#include "csolExtra.hpp"
#include "../csdefine.h"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "cellapp/move_controller.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

PY_SCRIPT_CONVERTERS( Entity )

GameObject::GameObject(Entity& e) : entity_(e)
{
    INFO_MSG("init GameObject extra (Entity: %d)\n", entity_.id());

	index_state_ = getPropertyLocalIndex("state");
    index_GameObject_utype_ = getPropertyLocalIndex("utype");
	index_GameObject_flags_ = getPropertyLocalIndex("flags");
	index_GameObject_tempMapping_ = getPropertyLocalIndex("tempMapping");
    boundingBoxHZ_ = getBoundingBox().z/2;
    boundingBoxHX_ = getBoundingBox().x/2;
	index_GameObject_planesID_ = getPropertyLocalIndex( "planesID" );
}

GameObject::~GameObject()
{
}

/**
 *  获取实体到地面的碰撞点
 *
 *  @param  position  返回地面的碰撞点
 *
 *  @return
 *
 */
bool GameObject::getDownToGroundPos(Vector3 &position)
{
    const Vector3 &pos = entity_.position();
    bool xOK = -10000 < pos.x && pos.x < 10000;
    bool yOK = -10000 < pos.y && pos.y < 10000;
    bool zOK = -10000 < pos.z && pos.z < 10000;
    if (! ( xOK && yOK && zOK ))
    {
        ERROR_MSG("Unexpected huge coordinate, Entity: %d, position = %f,%f,%f \n",
                entity_.id(), entity_.position().x, entity_.position().y,
                entity_.position().z);
        return false;
    }

	ChunkSpace *pSpace = entity_.pChunkSpace();

    Vector3 src = pos + Vector3(0.0f, 0.1f, 0.0f);
    Vector3 dst = pos + Vector3(0.0f, -10.f, 0.0f);

	//用ClosestObstacle，取第一个碰撞点
    float dist = pSpace->collide(src, dst, ClosestObstacle::s_default);
    if (dist < 0.f)
    {
        position = pos;
        return true;
    }

    Vector3 dir = dst - src;
    dir.normalise();
    position = src + dir * dist;

    return  true;
}

/**
 * 关于位面新加的几个函数
 *  @param  entity
 *
 *  @return	tur/false
 *
 */
long GameObject::getPlanesID()
{
	PyObject *pPlanesID = entity_.propertyByLocalIndex( index_GameObject_planesID_ ).getObject();
	return PyInt_AsLong( pPlanesID );
}

bool GameObject::isSamePlanesExt(Entity * pEntity )
{
	GameObject *pGmaeObject = CsolExtra::extraProxy<GameObject *>( pEntity );
	if( this->getPlanesID() == pGmaeObject->getPlanesID() )
		return true;
	else
		return false;
}
	
PyObject* GameObject::entitiesInRangeExt( float fRange, PyObjectPtr pEntityName=NULL, PyObjectPtr pPos = NULL )
{
	ExtEntityReceiver entReceiver = ExtEntityReceiver();
	PyObject *pNewList = PyList_New(0);

	entity_.getEntitiesInRange( entReceiver, fRange, pEntityName, pPos );
	//needDisperse = entReceiver.entities().size() > 0;
	GameObject * pEntAround;
	for( EntityList::iterator it = entReceiver.begin(); it != entReceiver.end(); it++ )
	{
		pEntAround = CsolExtra::extraProxy<GameObject *>( *it );
		if( pEntAround != NULL && this->isSamePlanesExt( *it ))
		{
			PyList_Append( pNewList, *it );
		}
	}
	
	return pNewList;
}

// 转换position为当前entity所在空间的地面点
void GameObject::transToGroundPosition( Vector3 &position )
{
	Vector3 srcPos = position + Vector3(0.0f, 5.f, 0.0f);
	Vector3 dstPos = position + Vector3(0.0f, -10.f, 0.0f);

	float collideDist = entity_.pChunkSpace()->collide(srcPos, dstPos, ClosestObstacle::s_default);
    if (collideDist < 0.f)
    {
        return;
    }
	position = Vector3( srcPos.x, srcPos.y - collideDist, srcPos.z );
}

/**
 * 获取boundingBox，只是简单地调用脚本的方法
 */
Vector3 GameObject::getBoundingBox()
{
	Vector3 v(0.0f, 0.0f, 0.0f);

    PyObject *pGetBoundingBoxStr = PyString_FromString("getBoundingBox");
    PyObject *boundingbox = PyObject_CallMethodObjArgs((PyObject *)&entity_, pGetBoundingBoxStr , NULL );
    if (PyVector<Vector3>::Check(boundingbox))
    {
        v = ((PyVector<Vector3>*)boundingbox)->getVector();
    }

    Py_XDECREF(pGetBoundingBoxStr);
    Py_XDECREF(boundingbox);
    return v;
}

/**
 * 获取boundingBox的z坐标的一半
 */
float GameObject::BoundingBoxHZ()
{
    if(boundingBoxHZ_ == 0.0f)
        boundingBoxHZ_ = getBoundingBox().z/2;
    return boundingBoxHZ_;
}

/**
 * 获取boundingBox的x坐标的一半
 */
float GameObject::BoundingBoxHX()
{
    if(boundingBoxHX_ == 0.0f)
        boundingBoxHX_ = getBoundingBox().x/2;
    return boundingBoxHX_;
}

/*
** 获取Entity.position对应的地表点，开放给脚本的接口
*/
PyObject *GameObject::getDownToGroundPos_cpp()
{

	Vector3 position;
	if( getDownToGroundPos( position ) ){
		return Script::getData( position );
	}
	else{
		Py_XINCREF( Py_None );
		return Py_None;
	}
}

/**
 *  导出的脚本接口
 *  计算实体之间的距离
 *
 *  @param pEntity 实体对象指针
 *
 *  @return 返回距离
 *
 */
float GameObject::distanceBB_cpp(Entity * pEntity)
{
    if( pEntity == NULL )								//如果脚本调用时传入一个None，则pEntity就是NULL，视为无效
    	return FLT_MAX;

	GameObject *gameObject = static_cast<GameObject *>( CsolExtra::instance( *pEntity ).getMapInstancePtr() );
    float s1 = this->BoundingBoxHZ();
    float d1 = gameObject->BoundingBoxHZ();

    const Vector3 &selfPos = entity_.position();
    const Vector3 &dstPos = pEntity->position();

    if(this->hasState() && gameObject->hasState())
    {
        int64 self_flags = this->flags();
        int64 dst_flags = gameObject->flags();

        if((self_flags & ( 1 << ROLE_FLAG_FLY)) ||
                (dst_flags & ( 1 << ROLE_FLAG_FLY)))
        {
            return (selfPos - dstPos).length() - s1 -d1;
        }
    }

    Vector3 selfPosGround, dstPosGround;
    bool selfst = this->getDownToGroundPos(selfPosGround);
    bool dstst = gameObject->getDownToGroundPos(dstPosGround);

    if(!selfst)
        selfPosGround = selfPos;
    if(!dstst)
        dstPosGround = dstPos;

    return (selfPosGround - dstPosGround).length() - s1 -d1;
}

/**
 *  查询临时记录
 *
 *  @param pKey 字典索引指针
 *  @param pDefValue 默认值指针
 *
 *  @return 如果存在，返回字典索引对应的值，否则返回默认值
 *
 */
 PyObject * GameObject::queryTemp_cpp( PyObjectPtr pCppKey, PyObjectPtr pCppDefValue )
{
	/*
	* tempMapping属性不能用propertyByLocalIndex获取，使用该方式得到的指针并非真实指向tempMapping
	* 的指针（与PyObject_GetAttrString( (PyObject *)&entity_, "tempMapping" )得到的指针所指向的地
	* 址不相同），对该指针执行PyDict_Contains操作会导致cell崩溃。
	* 后注：该问题的原因已查明，是因为通过propertyByLocalIndex方法获取属性应该传入属性的localIndex
	* 而不是index。
	*/
	PyObject * pKey = Script::getData( pCppKey );
	PyObject * pTempMapping = entity_.propertyByLocalIndex(index_GameObject_tempMapping_).getObject();
	if( NULL == pTempMapping ){
		PyErr_Format(PyExc_TypeError, "cellextra::GameObject::queryTemp_cpp:got a NULL tempMapping, entity(id:%lu) is a %s.\n", \
			entity_.id(), entity_.isReal() ? "real" : "ghost");
	}
	else{
		if(PyDict_Contains( pTempMapping, pKey ) == 1){
			PyObject *pValue = PyDict_GetItem( pTempMapping, pKey );
			//Py_XDECREF(pTempMapping);
			Py_XDECREF(pKey);
			return pValue;
		}
	}
	//Py_XDECREF(pTempMapping);
	Py_XDECREF(pKey);
	return pCppDefValue.getObject();
}

/**
 * 设置key是字符串类型，值是整数类型的临时属性，该接口不开放给脚本
 */
void GameObject::setTemp( const char * key, int val )
{
	PyObject * pTempMapping = entity_.propertyByLocalIndex(index_GameObject_tempMapping_).getObject();
	if( pTempMapping == NULL )
	{
		PyErr_Format(PyExc_TypeError, "cellextra::GameObject::setTemp:got a NULL tempMapping, entity(id:%lu) is a %s.\n", \
			entity_.id(), entity_.isReal() ? "real" : "ghost");
	}
	else
	{
		PyObject * pVal = PyInt_FromLong(val);
		PyDict_SetItemString(pTempMapping, key, pVal);
		Py_XDECREF(pVal);
	}
}

/**
 * 查询key是字符串类型，值是整数类型的临时属性，该接口不开放给脚本
 */
int GameObject::queryTemp( const char * key, int defVal )
{
	PyObject * pTempMapping = entity_.propertyByLocalIndex(index_GameObject_tempMapping_).getObject();
	if( pTempMapping != NULL )
	{
		PyObject * result = PyDict_GetItemString( pTempMapping, key );
		if( result != NULL )
			return PyInt_AsLong( result );
	}
	else
	{
		PyErr_Format(PyExc_TypeError, "cellextra::GameObject::queryTemp:got a NULL tempMapping, entity(id:%lu) is a %s.\n", \
			entity_.id(), entity_.isReal() ? "real" : "ghost");
	}
	return defVal;
}

/**
 *  获取def定义的属性索引
 *
 *  @param name 属性名称
 *
 *  @return -1表示没有这个属性
 */
int GameObject::getPropertyIndex(const std::string &name)
{
	EntityTypePtr type = entity_.pType();
	const EntityDescription &entdesc =  type->description();

    DataDescription *datadesc = entdesc.findProperty(name);
    if(datadesc == NULL)
    {
        ERROR_MSG("Entity %s not Property [%s]\n",entdesc.name().c_str(), name.c_str());
        return -1;
    }
    else
        return datadesc->index();
}

/**
 *  获取def定义的属性索引。注意：只有在def文件中定义的属性才有localIndex，
 *  其它属性没有，但是可以使用PyObject_GetAttrString获取
 *
 *  @param name 属性名称
 *
 *  @return -1表示没有这个属性
 */
int GameObject::getPropertyLocalIndex(const char * name) const
{
    DataDescription *datadesc = entity_.pType()->description( name );
    if(datadesc == NULL)
    {
        ERROR_MSG("Entity '%s' has no property [%s]\n",entity_.pType()->name(), name );
        return -1;
    }
    else
        return datadesc->localIndex();
}

/**
 *  直线移动到目标点，如果移动路线中有障碍物则停下
 *
 *	@param destination		The location to which we should move
 *	@param velocity			Velocity in metres per second
 *	@param crossHight		Ignoring the barrier height
 *  @param distance			Stop once we collide with obstacle within this range
 *	@param faceMovement		Whether or not the entity should face in the
 *							direction of movement.
 *	@param moveVertically	Whether or not the entity should move vertically.
 *
 */
PyObject *GameObject::moveToPointObstacle_cpp( Vector3 destination,
							float velocity,
							int userArg,
							float crossHight,	// 跨越障碍的高度
							float distance,		// 碰撞反弹的距离,避免表现上entity嵌入碰撞体内部
							bool faceMovement,
							bool moveVertically )
{
	if( !entity_.isReal() )
	{
		PyErr_SetString( PyExc_TypeError,
				"Entity.moveToPointObstacle_cpp can only be called on a real entity" );
		return Script::getData( -1 );
	}

	// 考虑跨越障碍的高度，默认为玩家的爬坡高度0.5米
	Vector3 adrustSrcPos = entity_.position() + Vector3(0.0f, crossHight, 0.0f);
	Vector3 adrustDstPos = destination + Vector3(0.0f, crossHight, 0.0f);

	float dist = entity_.pChunkSpace()->collide(adrustSrcPos, adrustDstPos, ClosestObstacle::s_default );
	if( dist > 0.f )
	{
		Vector3 dir = adrustDstPos - adrustSrcPos;
		dir.normalise();
		adrustDstPos = adrustSrcPos + dir * ( dist - distance );
	}
	transToGroundPosition( adrustDstPos );
	return Script::getData( entity_.addController( new MoveToPointController( adrustDstPos, "", 0, velocity, faceMovement, moveVertically ), userArg ) );
}

std::string GameObject::testPropertyIndex( const char * name )
{
	std::ostringstream report;
	int index = getPropertyIndex(name);
	int localIndex = getPropertyLocalIndex(name);
	PyObject * pProperty = PyObject_GetAttrString( (PyObject *)&entity_, name );
	DEBUG_MSG( "Test for property %s:\n", name );
	report << "Test for property " << name << " :\n";
	DEBUG_MSG( "	index is %d, value is %d\n", index, entity_.propertyByLocalIndex(index).getObject() );
	report << "	index is " << index << ", value is " << entity_.propertyByLocalIndex(index).getObject() << "\n";
	DEBUG_MSG( "	localIndex is %d, value is %d\n", localIndex, entity_.propertyByLocalIndex(localIndex).getObject() );
	report << "	localIndex is " << localIndex << ", value is " << entity_.propertyByLocalIndex(localIndex).getObject() << "\n";
	DEBUG_MSG( "	value by PyObject_GetAttrString is %d\n", pProperty );
	report << "	value by PyObject_GetAttrString is " << pProperty << "\n";
	DEBUG_MSG( "Test for property %s end.\n", name );
	report << "Test for property " << name << " end.\n";
	Py_XDECREF( pProperty );
	return report.str();
}




