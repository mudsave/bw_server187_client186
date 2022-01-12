#ifndef GAMEOBJECT_HPP
#define GAMEOBJECT_HPP

#include "cellapp/entity.hpp"
#include "pyscript/script_math.hpp"

PY_SCRIPT_CONVERTERS_DECLARE( Entity );

namespace{
	const float ROLE_AOI_RADIUS = 50.0;	 //entity的视野距离

	typedef std::vector<Entity *> EntityList;

	class ExtEntityReceiver: public Entity::EntityReceiver
	{
	public:
		~ExtEntityReceiver() { entities_.clear(); }
		void addEntity( Entity * pEntity )
		{
			entities_.push_back( pEntity );
		}
		int size()
		{
			return entities_.size();
		}
		EntityList::iterator begin() { return entities_.begin(); }
		EntityList::iterator end() { return entities_.end(); }
		void clear() { entities_.clear(); }
		const EntityList & entities() const { return entities_; }
	private:
		EntityList entities_;
	};
}

class GameObject
{

public:
	GameObject(Entity& e);
	~GameObject();

public:
	//PY_AUTO_METHOD_DECLARE( RETOWN, getDownToGroundPos_GameObject_cpp, END );
	PyObject *getDownToGroundPos_GameObject_cpp();

    virtual float distanceBB_cpp(Entity * pEntity);
	virtual bool getDownToGroundPos(Vector3 &position);
	virtual Vector3 getBoundingBox();
    virtual float BoundingBoxHZ();
    virtual float BoundingBoxHX();
	virtual PyObject *queryTemp_cpp(PyObjectPtr, PyObjectPtr);
	virtual PyObject *getDownToGroundPos_cpp();

	virtual PyObject * moveToPointObstacle_cpp( Vector3 destination,
							float velocity,
							int userArg = 0,
							float crossHight = 0.5,
							float distance = 0.5,
							bool faceMovement = true,
							bool moveVertically = false );
public:
	void setTemp(const char *, int);							//用于设置key是字符串类型，值是整数类型的临时变量
	int queryTemp(const char *, int);							//用于查询key时字符串类型，值是整数类型的临时变量

    bool hasState() { return index_state_ == -1 ? false:true; }

    inline int64 flags()
    {
        if(index_GameObject_flags_ == -1)
            return 0;

        PyObjectPtr o = entity_.propertyByLocalIndex(index_GameObject_flags_);
        return PyLong_AsLongLong(o.getObject());
    }

    inline bool hasFlag( int64 flag )
    {
    	return (flags() & (1 << flag)) != 0;
    }

    inline int8 utype()
    {
        if(index_GameObject_utype_ == -1)
            return 0;

        PyObjectPtr o = entity_.propertyByLocalIndex(index_GameObject_utype_);
        int8 ret = PyInt_AsLong(o.getObject());
        return ret;
    }


	void transToGroundPosition( Vector3 &position );
	
	long getPlanesID();
	bool isSamePlanesExt( Entity * pEntity );
	PyObject* entitiesInRangeExt( float fRange, PyObjectPtr pEntityName, PyObjectPtr pPos );

    //获取def定义的属性索引
    int getPropertyIndex(const std::string &name);
    int getPropertyLocalIndex( const char * name ) const;
    std::string testPropertyIndex( const char * name );

	//cell_public和all_clients属性
	int index_state_;		//state并不是GameObject层的属性，但是脚本中为了简便把子类的功能也写到了GameObject中，这里只能原样移植
	int index_GameObject_flags_;
    int index_GameObject_utype_;
	int index_GameObject_planesID_;
	//cell_private属性，注意ghost不能用到这类属性
	int index_GameObject_tempMapping_;

protected:
    float boundingBoxHZ_;								//实体模型绑定盒z坐标
    float boundingBoxHX_;								//实体模型绑定盒x坐标

	Entity & entity()					{ return entity_; }
	const Entity & entity() const		{ return entity_; }

	Entity & entity_;


};


#endif // GAMEOBJECT_HPP
