#ifndef MONSTER_HPP
#define MONSTER_HPP

#include "npcobject.hpp"
#include "../csdefine.h"


class Monster : public NPCObject
{

public:
	Monster(Entity& e);
	~Monster();

	//PY_AUTO_METHOD_DECLARE(RETDATA, queryRelation_Monster_cpp, ARG(PyObjectPtr, END));
    int queryRelation_Monster_cpp(PyObjectPtr pEntity);

    inline int state()
    {
        if(index_Monster_state_ == -1)
            return 0;

        PyObjectPtr o = entity_.propertyByLocalIndex(index_Monster_state_);
        int ret = PyInt_AsLong(o.getObject());
        return ret;
    }

    inline uint32 effect_state()
    {
        if(index_Monster_effect_state_ == -1)
            return 0;

        PyObjectPtr o = entity_.propertyByLocalIndex(index_Monster_effect_state_);
        uint32 ret = PyInt_AsUnsignedLongMask(o.getObject());
        return ret;
    }

    inline const char *etype()
    {

        if(utype() == ENTITY_TYPE_PET)
        {
            if(pGetOwnerStr_ == NULL)
                pGetOwnerStr_ = PyString_FromString("getOwner");

            PyObject *owner = PyObject_CallMethodObjArgs((PyObject *)&entity_, pGetOwnerStr_, NULL);

            if(owner == NULL)
                return "";

            PyObject *etype = PyObject_GetAttrString(owner, "etype");
            if(etype == NULL)
            {
                Py_XDECREF(owner);
                return "";
            }

            Py_XDECREF(owner);
            Py_XDECREF(etype);

            const char *s = PyString_AsString(etype);

            return s;
        }
        else
        {
            return NULL;
        }
    }

    inline uint16 battleCamp()
    {
        if(index_Monster_battleCamp_ == -1)
            return 0;

        PyObjectPtr o = entity_.propertyByLocalIndex(index_Monster_battleCamp_);
        uint16 ret = PyInt_AsUnsignedLongMask(o.getObject());

        return ret;
    }

    inline int32 ownerID()
    {
        if(index_Monster_ownerID_ == -1)
            return 0;

        PyObject * o = entity_.propertyByLocalIndex(index_Monster_ownerID_).getObject();
        if( o == NULL )
        {
        	PyErr_Format(PyExc_TypeError, "cellextra::Monster::ownerID:got a NULL ownerID, entity(id:%lu) is a %s.\n", \
			entity_.id(), entity_.isReal() ? "real" : "ghost");
			return 0;
        }
        else
        {
        	return PyInt_AsLong(o);
        }
    }

    inline bool isDestroyed()
    {
        return entity_.isDestroyed();
    }

    inline bool queryTemp()
    {

        PyObject *pRet = PyObject_CallMethod((PyObject *)&entity_, "queryTemp",
                "(s, i)", "is_dart_banditti", 0);
        if(pRet == NULL)
            return false;

        bool ret = (pRet == Py_True) ? true: false;

        Py_XDECREF(pRet);

        return ret;
    }
    using NPCObject::queryTemp;

    inline bool actionSign( int32 signWord )
    {
    	PyObjectPtr o = entity_.propertyByLocalIndex(index_Monster_actWord_);
    	return (PyInt_AsLong(o.getObject()) & signWord) != 0;
    }

    inline ObjectID targetID()
    {
    	PyObjectPtr o = entity_.propertyByLocalIndex(index_Monster_targetID_);
    	return PyInt_AsLong(o.getObject());
    }

    inline bool isMoving()
    {
    	PyObjectPtr o = entity_.propertyByLocalIndex(index_Monster_movingControlID_);
    	return PyInt_AsLong(o.getObject()) != 0;
    }

private:
    int commonRelationCheck(Monster *extra, uint32 args_effect);
    int queryRelation_Monster1(Entity *pEntity);
    int queryRelation_Monster2(Entity *pEntity, long args_utype);

    PyObject *pGetOwnerStr_;

    bool hasQueryTemp_;
	//cell_public和all_clients属性
    int index_Monster_state_;
    int index_Monster_effect_state_;
    int index_Monster_battleCamp_;
    int index_Monster_actWord_;
    int index_Monster_movingControlID_;
    //OTHER_CLIENTS
    int index_Monster_targetID_;
    //比较特殊的属性，这个属性是脚本Monster的子类型的属性
    //而且这个属性在不同的子类型中有不同的定义，包括CELL_PUBLIC、
    //ALL_CLIENTS、CELL_PRIVATE、OTHER_CLIENTS
    int index_Monster_ownerID_;
};


#endif // MONSTER_HPP
