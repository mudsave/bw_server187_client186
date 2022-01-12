#include "npcobject.hpp"
#include "csolExtra.hpp"


NPCObject::NPCObject( Entity &e ):GameObject(e)
{
    index_modelScale_ = getPropertyLocalIndex("modelScale");
    index_modelNumber_ = getPropertyLocalIndex("modelNumber");
    modelScale_ = 0.0f;
    modelNumber_ = "";
}

float NPCObject::BoundingBoxHZ()
{
	if( modelScaleChanged() || modelNumberChanged() )
		boundingBoxHZ_ = getBoundingBox().z/2;
	return boundingBoxHZ_;
}

float NPCObject::BoundingBoxHX()
{
	if( modelScaleChanged() || modelNumberChanged() )
		boundingBoxHX_ = getBoundingBox().x/2;
	return boundingBoxHX_;
}
