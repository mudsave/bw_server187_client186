#ifndef NPCOBJECT_HPP
#define NPCOBJECT_HPP

#include "gameobject.hpp"

class NPCObject : public GameObject
{
public:
	NPCObject( Entity &e );
	float BoundingBoxHZ();								//重写获取boudingbox的方法，根据模型缩放比例进行计算
	float BoundingBoxHX();								//重写获取boudingbox的方法，根据模型缩放比例进行计算
public:
	//get current modelScale
	inline float currModelScale()
	{
		if(index_modelScale_ == -1){
			index_modelScale_ = getPropertyLocalIndex("modelScale");
			ERROR_MSG( "NPCObject::currModelScale:index_modelScale_ init failed.redo." );
		}
		return PyFloat_AsDouble( entity_.propertyByLocalIndex(index_modelScale_).getObject() );
	}
	//get current modelNumber
	inline std::string currModelNumber()
	{
		if(index_modelNumber_ == -1){
			index_modelNumber_ = getPropertyLocalIndex("modelNumber");
			ERROR_MSG( "NPCObject::currModelScale:index_modelNumber_ init failed.redo." );
		}
		return PyString_AsString( entity_.propertyByLocalIndex(index_modelNumber_).getObject() );
	}
private:
	//check if modelScale is changed
	inline bool modelScaleChanged()
	{
		float currModelScale_ = currModelScale();
		if( (currModelScale_-modelScale_) > 0.001 || (currModelScale_-modelScale_) < -0.001 )
		{
			modelScale_ = currModelScale_;
			return true;
		}
		else
			return false;
	}
	//check if modelNumber is changed
	inline bool modelNumberChanged()
	{
		std::string currModelNumber_ = currModelNumber();
		if( currModelNumber_ != modelNumber_ )
		{
			modelNumber_ = currModelNumber_;
			return true;
		}
		else
			return false;
	}
private:
	float modelScale_;									//模型缩放比例
	std::string modelNumber_;							//模型编号
	//cell_public和all_clients属性
	int index_modelScale_;								//模型缩放比例属性的索引
	int index_modelNumber_;								//模型编号属性的索引
	//cell_private属性，注意ghost不能用到这类属性
};

#endif			//NPCOBJECT_HPP
