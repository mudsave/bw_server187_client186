#include "monster.hpp"
#include "csolExtra.hpp"
#include "cellapp/cellapp.hpp"

DECLARE_DEBUG_COMPONENT(0)


Monster::Monster(Entity& e) : NPCObject(e)
{
	INFO_MSG("init Monster extra (Entity: %d)\n", entity_.id());
    pGetOwnerStr_   = NULL;
    index_Monster_state_= getPropertyLocalIndex("state");
    index_Monster_effect_state_ = getPropertyLocalIndex("effect_state");
    index_Monster_battleCamp_ = getPropertyLocalIndex("battleCamp");
    index_Monster_ownerID_ = getPropertyLocalIndex("ownerID");
    index_Monster_actWord_ = getPropertyLocalIndex("actWord");
    index_Monster_targetID_ = getPropertyLocalIndex("targetID");
    index_Monster_movingControlID_ = getPropertyLocalIndex("movingControlID");
}

Monster::~Monster()
{
    if(pGetOwnerStr_ == NULL)
        Py_XDECREF(pGetOwnerStr_);
}

/**
 * 关系判断公共使用部分
 */
int Monster::commonRelationCheck(Monster *extra, uint32 args_effect)
{
    if( isDestroyed() == true || extra->isDestroyed() == true)
        return RELATION_NOFIGHT;

    int32 args_state = extra->state();

    if(args_state == ENTITY_STATE_PENDING || \
            args_state == ENTITY_STATE_DEAD \
            || args_state == ENTITY_STATE_QUIZ_GAME)
        return RELATION_NOFIGHT;

    uint32 self_effect_state = effect_state();

    //全体免战判定
    if((self_effect_state & EFFECT_STATE_ALL_NO_FIGHT) || \
            (args_effect & EFFECT_STATE_ALL_NO_FIGHT))
        return RELATION_NOFIGHT;

    return RELATION_ANTAGONIZE;
}
int Monster::queryRelation_Monster2(Entity *pEntity, long args_utype)
{
	//Monster *monster = static_cast<Monster *>( CsolExtra::instance( *pEntity ).getMapInstancePtr() );
	Monster *monster = CsolExtra::extraProxy<Monster *>( pEntity );
	if( monster == NULL )
		return RELATION_NOFIGHT;

    int64 flag = flags();
    if(args_utype == 0)
        args_utype = monster->utype();

    uint32 args_effect_state = monster->effect_state();

    if(args_utype == ENTITY_TYPE_ROLE)
    {
        if(commonRelationCheck(monster, args_effect_state) == RELATION_NOFIGHT)
            return RELATION_NOFIGHT;

        //GM观察者模式 UINT32
        if(args_effect_state & EFFECT_STATE_WATCHER)
            return RELATION_NOFIGHT;

        if(flag & ( 1 << ENTITY_FLAG_CANT_BE_HIT_BY_ROLE))
            return RELATION_NOFIGHT;

        return RELATION_ANTAGONIZE;
    }

    if(commonRelationCheck(monster, args_effect_state) == RELATION_NOFIGHT)
        return RELATION_NOFIGHT;

    if(args_utype == ENTITY_TYPE_MONSTER || args_utype == ENTITY_TYPE_NPC || \
            args_utype == ENTITY_TYPE_CONVOY_MONSTER)
    {
        int self_battleCamp = battleCamp();
        int args_battleCamp = monster->battleCamp();

        if(self_battleCamp != 0 ||  args_battleCamp != 0)
        {
            if(self_battleCamp == args_battleCamp)
                return RELATION_FRIEND;
            else
                return RELATION_ANTAGONIZE;
        }

        int64 args_flags = monster->flags();

        if((args_flags & (1 << ENTITY_FLAG_CAN_BE_HIT_BY_MONSTER)) || \
                (flag & (1 << ENTITY_FLAG_CAN_BE_HIT_BY_MONSTER)))
            return RELATION_ANTAGONIZE;

        return RELATION_FRIEND;
    }

    if(args_utype == ENTITY_TYPE_SLAVE_MONSTER || args_utype == ENTITY_TYPE_VEHICLE_DART || \
            args_utype == ENTITY_TYPE_PANGU_NAGUAL)
    {

        if(this->queryTemp() == true)
            return RELATION_ANTAGONIZE;

        int32 args_owner_id = monster->ownerID();

        if(args_owner_id == 0)
            return RELATION_ANTAGONIZE;

        CellApp *cellapp = CellApp::pInstance();
	    Entity *ent = cellapp->findEntity(args_owner_id);
        if(ent != NULL)
            return queryRelation_Monster1(ent);

        return RELATION_ANTAGONIZE;
    }

    if((args_utype == ENTITY_TYPE_YAYU) || (args_utype == ENTITY_TYPE_CALL_MONSTER))
        return RELATION_ANTAGONIZE;

    return RELATION_FRIEND;
}

int Monster::queryRelation_Monster1(Entity *pEntity)
{
    long  args_utype;
    //检查当前实体是否是Monster
    if(memcmp(entity_.pType()->name(), "Monster", 6) != 0)
        return RELATION_FRIEND;

    //Monster *monster = static_cast<Monster *>( CsolExtra::instance( *pEntity ).getMapInstancePtr() );
    Monster *monster = CsolExtra::extraProxy<Monster *>( pEntity );

    if(monster == NULL)
    {
        ERROR_MSG("queryRelation_Monster_cpp not init Extra (Entity: %d)\n", entity_.id());
        return 0;
    }

    args_utype = monster->utype();

    //it's a pet
    if(args_utype == ENTITY_TYPE_PET)
    {
        const char *argsetype = monster->etype();

        if(memcmp(argsetype, "MAILBOX", 7) == 0)
            return RELATION_NOFIGHT;

        if(pGetOwnerStr_ == NULL)
                pGetOwnerStr_ = PyString_FromString("getOwner");

        PyObject *owner = PyObject_CallMethodObjArgs((PyObject *)&entity_, pGetOwnerStr_, NULL);

        if(owner == NULL)
            return RELATION_FRIEND;

        PyObject *pArgsEntity = PyObject_GetAttrString(owner, "entity");
        if(pArgsEntity == NULL)
        {
            Py_XDECREF(owner);
            return RELATION_FRIEND;
        }

        Entity *entity = static_cast<Entity *>(pArgsEntity);
        int st = queryRelation_Monster2(entity, 0);

        Py_XDECREF(owner);
        Py_XDECREF(pArgsEntity);
        return st;
    }

    //把宠物的敌对比较转嫁给它的主人
    //虽然此关系未来可能会根据不同的状态或buff导致关系的改变，但当前并没有此需求
    return queryRelation_Monster2(pEntity, args_utype);
}
int Monster::queryRelation_Monster_cpp(PyObjectPtr pEntity)
{

    Entity *pArgsEnt = static_cast<Entity *>(pEntity.getObject());

    return queryRelation_Monster1(pArgsEnt);

}

