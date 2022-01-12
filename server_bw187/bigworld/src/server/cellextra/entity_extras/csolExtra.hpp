/**
 *	csolExtra作为entity extra方法和功能实现转接层而存在。
 *	csol的entity的继承层次多且复杂，如果按照bw extra的设计思想，
 *	只能针对某一个entity某一个方法进行处理，无法大规模的优化entity的
 *	脚本方法，实现的extra方法也无法类似多态应用，必须脚本再做一
 *	层包装。这个框架实现了把bw离散和扁平化的extra实现方式转
 *	化为对应于csol脚本层的继承体系，以便代码的复用和维护。
 *	Design by wangshufeng.
 */

#ifndef CSOL_CELL_EXTRA_CSOLEXTRA_HPP
#define CSOL_CELL_EXTRA_CSOLEXTRA_HPP

#include "gameobject.hpp"
#include "cellapp/entity_extra.hpp"
#include <string>

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_ENTITY_EXTRA


class CsolExtra : public EntityExtra
{
Py_EntityExtraHeader( CsolExtra );

public:
	CsolExtra( Entity &e );
	~CsolExtra();

	PyObject *pyGetAttribute( const char *attr );
	int pySetAttribute( const char *attr, PyObject *value );

	PY_AUTO_METHOD_DECLARE(RETDATA, distanceBB_cpp, ARG(Entity *, END));
	float distanceBB_cpp(Entity * pEntity);

	PY_AUTO_METHOD_DECLARE(RETDATA, queryTemp_cpp, ARG(PyObjectPtr, OPTARG(PyObjectPtr, PyObjectPtr(), END)));
	PyObject *queryTemp_cpp(PyObjectPtr, PyObjectPtr);

	PY_AUTO_METHOD_DECLARE( RETDATA, testPropertyIndex, ARG( char *, END) );
	std::string testPropertyIndex( const char * name );

	PY_AUTO_METHOD_DECLARE( RETOWN, getDownToGroundPos_cpp, END );
	PyObject *getDownToGroundPos_cpp();

	PY_AUTO_METHOD_DECLARE( RETOWN, moveToPointObstacle_cpp,
		ARG( Vector3, ARG( float, OPTARG( int, 0, OPTARG( float, 0.5,
		OPTARG( float, 0.5,	OPTARG( bool, true, OPTARG( bool, false, END ) ) ) ) ) ) ) );
	PyObject * moveToPointObstacle_cpp( Vector3 destination,
							float velocity,
							int userArg = 0,
							float crossHight = 0.5,
							float distance = 0.5,
							bool faceMovement = true,
							bool moveVertically = false );
	
	PY_AUTO_METHOD_DECLARE(RETDATA, isSamePlanesExt, ARG(Entity *, END));
	bool isSamePlanesExt( Entity * pEntity );
	
	PY_AUTO_METHOD_DECLARE( RETOWN, entitiesInRangeExt,ARG( float, OPTARG( PyObjectPtr, NULL, OPTARG( PyObjectPtr, NULL, END ) ) ) );
	PyObject* entitiesInRangeExt( float fRange, PyObjectPtr pEntityName=NULL, PyObjectPtr pPos = NULL);

	static const Instance<CsolExtra> instance;

	void initMapInstancePtr();
	inline GameObject *getMapInstancePtr() { return mapInstancePtr; }

	//这个模板方法如果不在类内定义（即在CPP文件定义），会导致编译出来的
	//so文件无法加载
	template<class T> static T extraProxy( Entity * pEntity )
	{
		return dynamic_cast<T>( CsolExtra::instance( *pEntity ).getMapInstancePtr() );
	}

private:
	GameObject *mapInstancePtr;
};

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_BASE

#endif
