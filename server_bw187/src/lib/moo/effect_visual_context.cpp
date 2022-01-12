/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "effect_visual_context.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

namespace Moo
{

template<typename ObjectType>
class FixedListAllocator
{
public:
	FixedListAllocator() :
	  offset_( 0 )
	{
	}
	typedef SmartPointer<ObjectType> ObjectPtr;
	typedef std::vector<ObjectPtr> Container;

	static FixedListAllocator& instance()
	{
		static FixedListAllocator s_instance;
		return s_instance;
	}
	
	ObjectType* alloc()
	{
		static uint32 timestamp = -1;
		if (!rc().frameDrawn(timestamp))
			offset_ = 0;
		if (offset_ >= container_.size())
		{
			container_.push_back( new ObjectType );
		}
		return container_[offset_++].getObject();
	}

private:
	Container	container_;
	uint32		offset_;
};

/**
 * Singleton accessor.
 */
EffectVisualContext& EffectVisualContext::instance()
{
	static EffectVisualContext inst;
	return inst;
}

/**
 * This method sets up the constant mappings to prepare for rendering a visual.
 */
void EffectVisualContext::initConstants()
{
	if (!overrideConstants_)
	{
		ConstantMappings::iterator it = constantMappings_.begin();
		ConstantMappings::iterator end = constantMappings_.end();

		while (it != end)
		{
			*(it->first) = it->second;
			it++;
		}
	}
}

void EffectVisualContext::fini()
{
	if (pRenderSet_)
	{
		delete pRenderSet_;
	}
	
	if (pNormalisationMap_.hasComObject())
	{
		pNormalisationMap_ = NULL;
	}
	
	constantMappings_.clear();
}

class RecordedVector4Array : public RecordedEffectConstant
{
public:
	RecordedVector4Array(){}
	void init(ID3DXEffect* pEffect, D3DXHANDLE constantHandle, uint32 nConstants)
	{
		pEffect_ = pEffect;
		constantHandle_ = constantHandle;
		nConstants_ = nConstants;
		if (constants_.size() < nConstants_)
		{
			constants_.resize( nConstants_ );
		}
	}
	void apply()
	{
			pEffect_->SetVectorArray( constantHandle_, &constants_.front(), nConstants_ );
			pEffect_->SetArrayRange( constantHandle_, 0, nConstants_ );
	}
	static RecordedVector4Array* get( ID3DXEffect* pEffect, D3DXHANDLE constantHandle, uint32 nConstants )
	{
		RecordedVector4Array* pRet = FixedListAllocator<RecordedVector4Array>::instance().alloc();
		pRet->init( pEffect, constantHandle, nConstants );
		return pRet;
	}
	ID3DXEffect* pEffect_;
	D3DXHANDLE constantHandle_;
	uint32 nConstants_;
	std::vector<Vector4> constants_;
};

class WorldPaletteConstant : public EffectConstantValue
{
public:

	void captureConstants( Vector4* pTransforms, uint32& nConstants )
	{
		nConstants = 0;
		NodePtrVector::iterator it = EffectVisualContext::instance().pRenderSet()->transformNodes_.begin();
		NodePtrVector::iterator end = EffectVisualContext::instance().pRenderSet()->transformNodes_.end();
		Matrix m;
		while (it != end)
		{
			XPMatrixTranspose( &m, &(*it)->worldTransform() );
			pTransforms[nConstants++] = m.row(0);
			pTransforms[nConstants++] = m.row(1);
			pTransforms[nConstants++] = m.row(2);
			it++;
		}
	}

	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (EffectVisualContext::instance().pRenderSet())
		{		
			static Vector4 transforms[256*3];
			uint32 nConstants = 0;
			captureConstants( transforms, nConstants );
			pEffect->SetVectorArray( constantHandle, transforms, nConstants );
			pEffect->SetArrayRange( constantHandle, 0, nConstants );
		}
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{ 
		RecordedVector4Array* pRecorder = NULL;
		if (EffectVisualContext::instance().pRenderSet())
		{
			uint32 nConstants = EffectVisualContext::instance().pRenderSet()->transformNodes_.size() * 3;
			pRecorder = RecordedVector4Array::get( pEffect, constantHandle, nConstants );
			captureConstants( &pRecorder->constants_.front(), nConstants );
		}
		return pRecorder;
	}
};

class RecordedMatrixConstant : public RecordedEffectConstant, public Aligned
{
public:
	RecordedMatrixConstant(){}
	void init( ID3DXEffect* pEffect, D3DXHANDLE constantHandle, const Matrix& transform )
	{
		pEffect_ = pEffect;
		constantHandle_ = constantHandle;
		transform_ = transform;
	}
	void apply()
	{
		pEffect_->SetMatrix( constantHandle_, &transform_ );
	}
	static RecordedMatrixConstant* get( ID3DXEffect* pEffect, D3DXHANDLE constantHandle, const Matrix& transform )
	{
		RecordedMatrixConstant* pRet = FixedListAllocator<RecordedMatrixConstant>::instance().alloc();
		pRet->init( pEffect, constantHandle, transform );
		return pRet;
	}
	ID3DXEffect* pEffect_;
	D3DXHANDLE constantHandle_;
	Matrix transform_;
};

class ViewProjectionConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetMatrix( constantHandle, &Moo::rc().viewProjection() );
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		return RecordedMatrixConstant::get( pEffect, constantHandle, Moo::rc().viewProjection() );
	}
};

class WorldConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			pEffect->SetMatrix( constantHandle, &EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform() );
		}
		else
		{
			pEffect->SetMatrix( constantHandle, &Matrix::identity );
		}

		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			return RecordedMatrixConstant::get( pEffect, constantHandle, 
				EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform() );
		}
		return RecordedMatrixConstant::get( pEffect, constantHandle, Matrix::identity );
	}
};

class InverseWorldConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		Matrix m;
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			m = EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform();
		}
		else
		{
			m = Matrix::identity;
		}
		
		m.invert();
		pEffect->SetMatrix( constantHandle, &m );

		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		Matrix m;
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			m = EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform();
		}
		else
		{
			m = Matrix::identity;
		}
		
		m.invert();
		return RecordedMatrixConstant::get( pEffect, constantHandle, m );
	}
};

class ViewConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetMatrix( constantHandle, &Moo::rc().view() );
		return true;
	}
};

class ProjectionConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetMatrix( constantHandle, &Moo::rc().projection() );
		return true;
	}
};

class ScreenConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		Vector4 v( rc().screenWidth(),
			rc().screenHeight(),
			rc().halfScreenWidth(),
			rc().halfScreenHeight() );
		pEffect->SetVector( constantHandle, &v );
		return true;
	}
};

class WorldViewConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			Matrix wv;
			wv.multiply( EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform(), 
				Moo::rc().view() );
			pEffect->SetMatrix( constantHandle, &wv );
		}
		else
		{
			pEffect->SetMatrix( constantHandle, &rc().view() );
		}
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			Matrix wv;
			wv.multiply( EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform(), 
				Moo::rc().view() );
			return RecordedMatrixConstant::get( pEffect, constantHandle, wv );
		}
		return RecordedMatrixConstant::get( pEffect, constantHandle, rc().view() );
	}
};

class WorldViewProjectionConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			Matrix wvp;
			wvp.multiply( EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform(), 
				Moo::rc().viewProjection() );
			pEffect->SetMatrix( constantHandle, &wvp );
		}
		else
		{
			pEffect->SetMatrix( constantHandle, &rc().viewProjection() );
		}
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (EffectVisualContext::instance().pRenderSet() &&
			!EffectVisualContext::instance().pRenderSet()->treatAsWorldSpaceObject_)
		{
			Matrix wvp;
			wvp.multiply( EffectVisualContext::instance().pRenderSet()->transformNodes_.front()->worldTransform(), 
				Moo::rc().viewProjection() );
			return RecordedMatrixConstant::get( pEffect, constantHandle, wvp );
		}
		return RecordedMatrixConstant::get( pEffect, constantHandle, rc().viewProjection() );
	}
};

class EnvironmentTransformConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{		
		Matrix tr( Moo::rc().view() );
		tr.translation(Vector3(0,0,0));
		tr.postMultiply( Moo::rc().projection() );	
		pEffect->SetMatrix( constantHandle, &tr );		
		
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{	
		Matrix tr( Moo::rc().view() );
		tr.translation(Vector3(0,0,0));
		tr.postMultiply( Moo::rc().projection() );	
		
		return RecordedMatrixConstant::get( pEffect, constantHandle, tr );
	}
};

class AmbientConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		if (Moo::rc().lightContainer())
			pEffect->SetVector( constantHandle, (Vector4*)(&Moo::rc().lightContainer()->ambientColour())  );
		else
			pEffect->SetVector( constantHandle, &Vector4( 1.f, 1.f, 1.f, 1.f ));
		return true;
	}
};

class RecordedDirectionalConstant : public RecordedEffectConstant
{
public:
	struct ShadDirLight
	{
		Vector3 direction_;
		Colour colour_;
	};
	RecordedDirectionalConstant()
	{
	}
	void init( ID3DXEffect* pEffect, uint32 nConstants, D3DXHANDLE baseHandle )
	{
		pEffect_ = pEffect;
		nConstants_ = nConstants;
		baseHandle_ = baseHandle;
	}
	void apply()
	{
		for (uint32 i = 0; i < nConstants_; i++)
		{
			pEffect_->SetValue( constants_[i].first, 
				&constants_[i].second, sizeof( ShadDirLight ) );
		}
		pEffect_->SetArrayRange( baseHandle_, 0, nConstants_ );
	}
	static RecordedDirectionalConstant* get( ID3DXEffect* pEffect, uint32 nConstants, D3DXHANDLE baseHandle )
	{
		RecordedDirectionalConstant* pRet = FixedListAllocator<RecordedDirectionalConstant>::instance().alloc();
		pRet->init( pEffect, nConstants, baseHandle );
		return pRet;
	}
	ID3DXEffect* pEffect_;
	typedef std::pair<D3DXHANDLE, ShadDirLight> DirConstant;
	DirConstant constants_[2];
	uint32 nConstants_;
	D3DXHANDLE baseHandle_;

};

class DirectionalConstant : public EffectConstantValue
{
public:
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		RecordedDirectionalConstant* pConstant = NULL;
		if (objectSpace_)
		{
			const Matrix& invWorld = EffectVisualContext::instance().invWorld();

			LightContainerPtr pContainer;
			if (diffuse_)
				pContainer = Moo::rc().lightContainer();
			else
				pContainer = Moo::rc().specularLightContainer();

			if (pContainer != NULL)
			{
				DirectionalLightVector& dLights = pContainer->directionals();
				if (dLights.size() > 0)
				{
					uint32 nLights =  2 < dLights.size() ? 2 : dLights.size();
					pConstant = RecordedDirectionalConstant::get(pEffect, nLights, constantHandle);
					for (uint32 i = 0; i < nLights; i++)
					{
						RecordedDirectionalConstant::ShadDirLight& constant = pConstant->constants_[i].second;
						invWorld.applyVector( constant.direction_, dLights[i]->worldDirection() );
						constant.direction_.normalise();
						constant.colour_ = dLights[i]->colour() * dLights[i]->multiplier();
						pConstant->constants_[i].first = pEffect->GetParameterElement( constantHandle, i );
					}
				}
			}
		}
		return pConstant;
	}

	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		struct ShadDirLight
		{
			Vector3 direction_;
			Colour colour_;
		};
		static ShadDirLight constant;

		const Matrix& invWorld = EffectVisualContext::instance().invWorld();

		LightContainerPtr pContainer;
		if (diffuse_)
		{
			pContainer = Moo::rc().lightContainer();
		}
		else
		{
			pContainer = Moo::rc().specularLightContainer();
		}

		if (pContainer != NULL)
		{

			DirectionalLightVector& dLights = pContainer->directionals();
			uint32 nLights =  2 < dLights.size() ? 2 : dLights.size();
			for (uint32 i = 0; i < nLights; i++)
			{
				D3DXHANDLE elemHandle = pEffect->GetParameterElement( constantHandle, i );
				if (elemHandle)
				{
					if (objectSpace_)
					{
						invWorld.applyVector( constant.direction_, dLights[i]->worldDirection() );
						constant.direction_.normalise();
					}
					else
					{
						constant.direction_ = dLights[i]->worldDirection();
					}
					constant.colour_ = dLights[i]->colour() * dLights[i]->multiplier();
					pEffect->SetValue( elemHandle, &constant, sizeof( constant ) );
				}
			}
			pEffect->SetArrayRange( constantHandle, 0, nLights );
		}
		return true;
	}
	DirectionalConstant( bool diffuse = true, bool objectSpace = false ) : 
		objectSpace_( objectSpace ),
		diffuse_( diffuse )
	{

	}
private:
	bool objectSpace_;
	bool diffuse_;
};

class RecordedOmniConstant : public RecordedEffectConstant
{
public:
	struct ShadPointLight
	{
		Vector3	position_;
		Colour	colour_;
		Vector2	attenuation_;
	};
	RecordedOmniConstant(){}
	void init( ID3DXEffect* pEffect, uint32 nConstants, D3DXHANDLE baseHandle )
	{
		pEffect_ = pEffect;
		nConstants_ = nConstants;
		baseHandle_ = baseHandle;
	}

	void apply()
	{
		for (uint32 i = 0; i < nConstants_; i++)
		{
			pEffect_->SetValue( constants_[i].first, 
				&constants_[i].second, sizeof( ShadPointLight ) );
		}
		pEffect_->SetArrayRange( baseHandle_, 0, nConstants_ );
	}
	static RecordedOmniConstant* get( ID3DXEffect* pEffect, uint32 nConstants, D3DXHANDLE baseHandle )
	{
		RecordedOmniConstant* pRet = FixedListAllocator<RecordedOmniConstant>::instance().alloc();
		pRet->init( pEffect, nConstants, baseHandle );
		return pRet;
	}
	ID3DXEffect* pEffect_;
	typedef std::pair<D3DXHANDLE, ShadPointLight> PointConstant;
	PointConstant constants_[4];
	uint32 nConstants_;
	D3DXHANDLE baseHandle_;
};

class OmniConstant : public EffectConstantValue
{
public:
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		const Matrix& invWorld = EffectVisualContext::instance().invWorld();
		float scale = EffectVisualContext::instance().invWorldScale();

		LightContainerPtr pContainer;
		uint32 maxLights;
		if (diffuse_)
		{
			maxLights = 4;
			pContainer = Moo::rc().lightContainer();
		}
		else
		{
			maxLights = 2;
			pContainer = Moo::rc().specularLightContainer();
		}

		RecordedOmniConstant* pConstant = NULL;
		if (pContainer != NULL)
		{
			OmniLightVector& oLights = pContainer->omnis();
			uint32 nLights = maxLights < oLights.size() ? maxLights : oLights.size();
			if (nLights > 0)
			{
				pConstant = RecordedOmniConstant::get( pEffect, nLights, constantHandle );
				for (uint32 i = 0; i < nLights; i++)
				{
					D3DXHANDLE elemHandle = pEffect->GetParameterElement( constantHandle, i );
					pConstant->constants_[i].first = elemHandle;
					RecordedOmniConstant::ShadPointLight& constant = pConstant->constants_[i].second;
					if (elemHandle)
					{
						float innerRadius = oLights[i]->worldInnerRadius();
						float outerRadius = oLights[i]->worldOuterRadius();
						if (objectSpace_)
						{
							innerRadius *= scale;
							outerRadius *= scale;
							invWorld.applyPoint( constant.position_, oLights[i]->worldPosition() );
						}
						else
						{
							constant.position_ = oLights[i]->worldPosition();
						}

						constant.colour_ = oLights[i]->colour() * oLights[i]->multiplier();
						constant.attenuation_.set( outerRadius, 1.f / ( outerRadius - innerRadius ) );
					}
				}
			}
		}
		return pConstant;
	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		struct ShadPointLight
		{
			Vector3 position_;
			Colour colour_;
			Vector2 attenuation_;
		};
		static ShadPointLight constant;

		const Matrix& invWorld = EffectVisualContext::instance().invWorld();
		float scale = EffectVisualContext::instance().invWorldScale();

		LightContainerPtr pContainer;
		uint32 maxLights;
		if (diffuse_)
		{
			maxLights = 4;
			pContainer = Moo::rc().lightContainer();
		}
		else
		{
			maxLights = 2;
			pContainer = Moo::rc().specularLightContainer();
		}
		if (pContainer != NULL)
		{
			OmniLightVector& oLights = pContainer->omnis();
			uint32 nLights = maxLights < oLights.size() ? maxLights : oLights.size();
			for (uint32 i = 0; i < nLights; i++)
			{
				D3DXHANDLE elemHandle = pEffect->GetParameterElement( constantHandle, i );
				if (elemHandle)
				{
					float innerRadius = oLights[i]->worldInnerRadius();
					float outerRadius = oLights[i]->worldOuterRadius();
					if (objectSpace_)
					{
						innerRadius *= scale;
						outerRadius *= scale;
						invWorld.applyPoint( constant.position_, oLights[i]->worldPosition() );
					}
					else
					{
						constant.position_ = oLights[i]->worldPosition();
					}

					constant.colour_ = oLights[i]->colour() * oLights[i]->multiplier();
					constant.attenuation_.set( outerRadius, 1.f / ( outerRadius - innerRadius ) );
					pEffect->SetValue( elemHandle, &constant, sizeof( constant ) );
				}
			}
			pEffect->SetArrayRange( constantHandle, 0, nLights );
		}
		return true;
	}
	OmniConstant( bool diffuse = true, bool objectSpace = false ):
		objectSpace_( objectSpace ),
		diffuse_( diffuse )
	{

	}
private:
	bool objectSpace_;
	bool diffuse_;
};

class RecordedSpotConstant : public RecordedEffectConstant
{
public:
	struct ShadSpotLight
	{
		Vector3 position;
		Colour colour;
		Vector3 attenuation;
		Vector3 direction;
	};
	RecordedSpotConstant() {}

	void init( ID3DXEffect* pEffect, uint32 nConstants, D3DXHANDLE baseHandle )
	{
		pEffect_ = pEffect;
		nConstants_ = nConstants;
		baseHandle_ = baseHandle;
	}
	void apply()
	{
		for (uint32 i = 0; i < nConstants_; i++)
		{
			pEffect_->SetValue( constants_[i].first, 
				&constants_[i].second, sizeof( ShadSpotLight ) );
		}
		pEffect_->SetArrayRange( baseHandle_, 0, nConstants_ );
	}
	static RecordedSpotConstant* get( ID3DXEffect* pEffect, uint32 nConstants, D3DXHANDLE baseHandle )
	{
		RecordedSpotConstant* pRet = FixedListAllocator<RecordedSpotConstant>::instance().alloc();
		pRet->init( pEffect, nConstants, baseHandle );
		return pRet;
	}
	ID3DXEffect* pEffect_;
	typedef std::pair<D3DXHANDLE, ShadSpotLight> SpotConstant;
	SpotConstant constants_[2];
	uint32 nConstants_;
	D3DXHANDLE baseHandle_;
};

class SpotDiffuseConstant : public EffectConstantValue
{
public:
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		const Matrix& invWorld = EffectVisualContext::instance().invWorld();
		float scale = EffectVisualContext::instance().invWorldScale();

		RecordedSpotConstant* pConstant = NULL;

		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
		{

			SpotLightVector& sLights = pContainer->spots();
			uint32 nLights = 2 < sLights.size() ? 2 : sLights.size();
			if (nLights > 0)
			{
				pConstant = RecordedSpotConstant::get( pEffect, nLights, constantHandle );
				for (uint32 i = 0; i < 2 && i < sLights.size(); i++)
				{
					D3DXHANDLE elemHandle = pEffect->GetParameterElement( constantHandle, i );
					pConstant->constants_[i].first = elemHandle;
					RecordedSpotConstant::ShadSpotLight& constant = pConstant->constants_[i].second;
					if (elemHandle)
					{
						float innerRadius = sLights[i]->worldInnerRadius();
						float outerRadius = sLights[i]->worldOuterRadius();
						if (!objectSpace_)
						{
							constant.position = sLights[i]->worldPosition();
							constant.direction = sLights[i]->worldDirection();
						}
						else
						{
							innerRadius *= scale;
							outerRadius *= scale;
							constant.position = invWorld.applyPoint( sLights[i]->worldPosition() );
							constant.direction = invWorld.applyVector( sLights[i]->worldDirection() );
						}

						constant.colour = sLights[i]->colour() * sLights[i]->multiplier();
						constant.attenuation.set( outerRadius, 1.f / ( outerRadius - innerRadius ), sLights[i]->cosConeAngle() );
					}
				}
			}
		}
		return pConstant;
	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		struct ShadSpotLight
		{
			Vector3 position;
			Colour colour;
			Vector3 attenuation;
			Vector3 direction;
		};

		static ShadSpotLight constant;
		
		const Matrix& invWorld = EffectVisualContext::instance().invWorld();
		float scale = EffectVisualContext::instance().invWorldScale();

		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
		{
			SpotLightVector& sLights = pContainer->spots();
			uint32 nLights = 2 < sLights.size() ? 2 : sLights.size();
			for (uint32 i = 0; i < 2 && i < sLights.size(); i++)
			{
				D3DXHANDLE elemHandle = pEffect->GetParameterElement( constantHandle, i );
				if (elemHandle)
				{
					float innerRadius = sLights[i]->worldInnerRadius();
					float outerRadius = sLights[i]->worldOuterRadius();
					if (!objectSpace_)
					{
						constant.position = sLights[i]->worldPosition();
						constant.direction = sLights[i]->worldDirection();
					}
					else
					{
						innerRadius *= scale;
						outerRadius *= scale;
						constant.position = invWorld.applyPoint( sLights[i]->worldPosition() );
						constant.direction = invWorld.applyVector( sLights[i]->worldDirection() );
					}

					constant.colour = sLights[i]->colour() * sLights[i]->multiplier();
					constant.attenuation.set( outerRadius, 1.f / ( outerRadius - innerRadius ), sLights[i]->cosConeAngle() );
					pEffect->SetValue( elemHandle, &constant, sizeof( constant ) );
				}
			}
			pEffect->SetArrayRange( constantHandle, 0, nLights );
		}
		return true;
	}
	SpotDiffuseConstant( bool objectSpace = false )
	: objectSpace_( objectSpace )
	{

	}
private:
	bool objectSpace_;
};

class RecordedIntConstant : public RecordedEffectConstant
{
public:
	RecordedIntConstant()
	{
	}
	void init( ID3DXEffect* pEffect, D3DXHANDLE constantHandle, int value )
	{
	  pEffect_ = pEffect;
	  constantHandle_ = constantHandle;
	  value_ = value;
	}
	void apply()
	{
		pEffect_->SetInt( constantHandle_, value_ );
	}
	static RecordedIntConstant* get( ID3DXEffect* pEffect, D3DXHANDLE constantHandle, int value )
	{
		RecordedIntConstant* pRet = FixedListAllocator<RecordedIntConstant>::instance().alloc();
		pRet->init( pEffect, constantHandle, value );
		return pRet;
	}
	ID3DXEffect* pEffect_;
	D3DXHANDLE constantHandle_;
	int value_;
};

class NDirectionalsConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
		{
			pEffect->SetInt( constantHandle, pContainer->directionals().size() );
		}
		else
		{
			pEffect->SetInt( constantHandle, 0 );
		}
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
		{
			return RecordedIntConstant::get( pEffect, constantHandle, pContainer->directionals().size() );
		}
		return  RecordedIntConstant::get( pEffect, constantHandle, 0 );
	}
};

class NOmnisConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
		{
			pEffect->SetInt( constantHandle, pContainer->omnis().size() );
		}
		else
		{
			pEffect->SetInt( constantHandle, 0 );
		}
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
			return RecordedIntConstant::get( pEffect, constantHandle, pContainer->omnis().size() );
		return RecordedIntConstant::get( pEffect, constantHandle, 0 );
	}
};

class NSpotsConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
			pEffect->SetInt( constantHandle, pContainer->spots().size() );
		else
			pEffect->SetInt( constantHandle, 0 );
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().lightContainer();
		if (pContainer != NULL)
			return RecordedIntConstant::get( pEffect, constantHandle, pContainer->spots().size() );
		return RecordedIntConstant::get( pEffect, constantHandle, 0 );
	}
};

class NSpecularDirectionalsConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().specularLightContainer();
		if (pContainer != NULL)
		{
			pEffect->SetInt( constantHandle, pContainer->directionals().size() );
		}
		else
		{
			pEffect->SetInt( constantHandle, 0 );
		}
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().specularLightContainer();
		if (pContainer != NULL)
			return RecordedIntConstant::get( pEffect, constantHandle, pContainer->directionals().size() );
		return RecordedIntConstant::get( pEffect, constantHandle, 0 );
	}
};

class NSpecularOmnisConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().specularLightContainer();
		if (pContainer != NULL)
			pEffect->SetInt( constantHandle, pContainer->omnis().size() );
		else
			pEffect->SetInt( constantHandle, 0 );
		return true;
	}
	RecordedEffectConstant* record(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		LightContainerPtr pContainer = Moo::rc().specularLightContainer();
		if (pContainer != NULL)
			return RecordedIntConstant::get( pEffect, constantHandle, pContainer->omnis().size() );
		return RecordedIntConstant::get( pEffect, constantHandle, 0 );
	}
};

class StaticLightingConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetBool( constantHandle, EffectVisualContext::instance().staticLighting() );
		return true;
	}
};

class CameraPosConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		Vector4 camPos;
		if (objectSpace_)
		{
			reinterpret_cast<Vector3&>(camPos) = EffectVisualContext::instance().invWorld().applyPoint( rc().invView()[ 3 ] );
		}
		else
		{
			camPos = rc().invView().row(3);
		}
		pEffect->SetVector( constantHandle, &camPos );
		return true;
	}
	CameraPosConstant( bool objectSpace = false )
	: objectSpace_( objectSpace )
	{

	}
private:
	bool objectSpace_;
};

class NormalisationMapConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetTexture( constantHandle, EffectVisualContext::instance().pNormalisationMap().pComObject() );

		return true;
	}
	NormalisationMapConstant()
	{

	}
private:
};


class TimeConstant : public EffectConstantValue
{
public:
	static void tick( float dTime )
	{
		time_ += dTime;
	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetFloat( constantHandle, time_ );

		return true;
	}
private:
	static float time_;

};

class FogConstant : public EffectConstantValue
{
public:
	FogConstant( bool fogStart )
	: fogStart_( fogStart )
	{

	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetFloat( constantHandle, fogStart_ ? rc().fogNear() : rc().fogFar() );

		return true;
	}
private:
	bool fogStart_;
};

class FogColourConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetVector( constantHandle, (D3DXVECTOR4*)&Colour(Material::standardFogColour()) );

		return true;
	}
};

class FogEnabledConstant : public EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetBool( constantHandle, (BOOL)EffectVisualContext::instance().fogEnabled() );		

		return true;
	}
private:
};

float TimeConstant::time_ = 0;

EffectVisualContext::EffectVisualContext() : 
	invWorldScale_( 1.f ),
	staticLighting_( false ),
	isOutside_( true ),
	overrideConstants_( false ),
	fogEnabled_( true )
{
	invWorld_.setIdentity();
	constantMappings_[ EffectConstantValue::get( "WorldPalette" ) ] = new WorldPaletteConstant;
	constantMappings_[ EffectConstantValue::get( "World" ) ] = new WorldConstant;
	constantMappings_[ EffectConstantValue::get( "WorldIT" ) ] = new InverseWorldConstant;	//for rt/shader
	constantMappings_[ EffectConstantValue::get( "ViewProjection" ) ] = new ViewProjectionConstant;
	constantMappings_[ EffectConstantValue::get( "WorldViewProjection" ) ] = new WorldViewProjectionConstant;
	constantMappings_[ EffectConstantValue::get( "View" ) ] = new ViewConstant;
	constantMappings_[ EffectConstantValue::get( "WorldView" ) ] = new WorldViewConstant;
	constantMappings_[ EffectConstantValue::get( "Projection" ) ] = new ProjectionConstant;
	constantMappings_[ EffectConstantValue::get( "Screen" ) ] = new ScreenConstant;
	constantMappings_[ EffectConstantValue::get( "EnvironmentTransform" ) ] = new EnvironmentTransformConstant;
	constantMappings_[ EffectConstantValue::get( "Ambient" ) ] = new AmbientConstant;
	constantMappings_[ EffectConstantValue::get( "DirectionalLights" ) ] = new DirectionalConstant;
	constantMappings_[ EffectConstantValue::get( "PointLights" ) ] = new OmniConstant;
	constantMappings_[ EffectConstantValue::get( "SpotLights" ) ] = new SpotDiffuseConstant;
	constantMappings_[ EffectConstantValue::get( "SpecularDirectionalLights" ) ] = new DirectionalConstant(false);
	constantMappings_[ EffectConstantValue::get( "SpecularPointLights" ) ] = new OmniConstant(false);
	constantMappings_[ EffectConstantValue::get( "CameraPos" ) ] = new CameraPosConstant;
	constantMappings_[ EffectConstantValue::get( "WorldEyePosition" ) ] = new CameraPosConstant; //for rt/shader
	constantMappings_[ EffectConstantValue::get( "DirectionalLightsObjectSpace" ) ] = new DirectionalConstant(true, true);
	constantMappings_[ EffectConstantValue::get( "PointLightsObjectSpace" ) ] = new OmniConstant(true, true);
	constantMappings_[ EffectConstantValue::get( "SpotLightsObjectSpace" ) ] = new SpotDiffuseConstant(true);
	constantMappings_[ EffectConstantValue::get( "SpecularDirectionalLightsObjectSpace" ) ] = new DirectionalConstant(false, true);
	constantMappings_[ EffectConstantValue::get( "SpecularPointLightsObjectSpace" ) ] = new OmniConstant(false, true);
	constantMappings_[ EffectConstantValue::get( "CameraPosObjectSpace" ) ] = new CameraPosConstant(true);
	constantMappings_[ EffectConstantValue::get( "DirectionalLightCount" ) ] = new NDirectionalsConstant;
	constantMappings_[ EffectConstantValue::get( "PointLightCount" ) ] = new NOmnisConstant;
	constantMappings_[ EffectConstantValue::get( "SpotLightCount" ) ] = new NSpotsConstant;
	constantMappings_[ EffectConstantValue::get( "SpecularDirectionalLightCount" ) ] = new NSpecularDirectionalsConstant;
	constantMappings_[ EffectConstantValue::get( "SpecularPointLightCount" ) ] = new NSpecularOmnisConstant;
	constantMappings_[ EffectConstantValue::get( "Time" ) ] = new TimeConstant;
	constantMappings_[ EffectConstantValue::get( "StaticLighting" ) ] = new StaticLightingConstant;	
	constantMappings_[ EffectConstantValue::get( "FogStart" ) ] = new FogConstant(true);
	constantMappings_[ EffectConstantValue::get( "FogEnd" ) ] = new FogConstant(false);
	constantMappings_[ EffectConstantValue::get( "FogColour" ) ] = new FogColourConstant;
	constantMappings_[ EffectConstantValue::get( "FogEnabled" ) ] = new FogEnabledConstant;

	createNormalisationMap();
	constantMappings_[ EffectConstantValue::get( "NormalisationMap" ) ] = new NormalisationMapConstant;
}

EffectVisualContext::~EffectVisualContext()
{
}

void EffectVisualContext::pRenderSet( Visual::RenderSet* pRenderSet )
{ 
	pRenderSet_ = pRenderSet; 
	if (pRenderSet_ && !pRenderSet_->treatAsWorldSpaceObject_)
	{
		invWorld_.invert( pRenderSet_->transformNodes_.front()->worldTransform() );
		invWorldScale_ = XPVec3Length( &Vector3Base( invWorld_ ) );
	}
	else
	{
		invWorld_ = Matrix::identity;
		invWorldScale_ = 1.f;
	}
}

void EffectVisualContext::tick( float dTime )
{
	TimeConstant::tick( dTime );	
}


const uint32 CUBEMAP_SIZE = 128;
const uint32 CUBEMAP_SHIFT = 7;

/**
 * This method helps with creating the normalisation cubemap.
 *
 * @param buffer pointer to the side of the cubemap we are filling in.
 * @param xMask the direction of the x-axis on this cubemap side.
 * @param yMask the direction of the y-axis on this cubemap side.
 * @param zMask the direction of the z-axis on this cubemap side.
 */
static void fillNormMapSurface( uint32* buffer, Vector3 xMask, Vector3 yMask, Vector3 zMask )
{
	for (int i = 0; i < int(CUBEMAP_SIZE*CUBEMAP_SIZE); i++)
	{
		int xx = i & (CUBEMAP_SIZE - 1);
		int yy = i >> CUBEMAP_SHIFT;
		float x = (( float(xx) / float(CUBEMAP_SIZE - 1) ) * 2.f ) - 1.f;
		float y = (( float(yy) / float(CUBEMAP_SIZE - 1) ) * 2.f ) - 1.f;
		Vector3 out = ( xMask * x ) + (yMask * y) + zMask;
		out.normalise();
		out += Vector3(1, 1, 1);
		out *= 255.5f * 0.5f;
		*(buffer++) = 0xff000000 |
			(uint32(out.x)<<16) |
			(uint32(out.y)<<8) |
			uint32(out.z);
	}
}

/**
 *	This method creates the normalisation cube map.
 */
void EffectVisualContext::createNormalisationMap()
{
	DX::CubeTexture* pNormalisationMap;
	HRESULT hr = rc().device()->CreateCubeTexture( CUBEMAP_SIZE, 1, 0, 
		D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pNormalisationMap, NULL );

	if ( FAILED( hr ) )
	{
		ERROR_MSG( "EffectVisualContext::createNormalisationMap - couldn't create cube texture.  error code %lx\n", hr );
	}

	// fill in all six cubemap sides.

	//positive x
	D3DLOCKED_RECT lockedRect;
	hr = pNormalisationMap->LockRect( D3DCUBEMAP_FACE_POSITIVE_X, 0, &lockedRect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		fillNormMapSurface( (uint32*)lockedRect.pBits, Vector3( 0, 0, -1 ), Vector3( 0, -1, 0), Vector3(  1, 0, 0 )  );
		pNormalisationMap->UnlockRect( D3DCUBEMAP_FACE_POSITIVE_X, 0 );
	}
	else
		ERROR_MSG( "EffectVisualContext::createNormalisationMap - couldn't get cube surface.  error code %lx\n", hr );

	//positive x
	hr = pNormalisationMap->LockRect( D3DCUBEMAP_FACE_NEGATIVE_X, 0, &lockedRect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		fillNormMapSurface( (uint32*)lockedRect.pBits, Vector3( 0, 0, 1 ), Vector3( 0, -1, 0), Vector3( -1, 0, 0 )  );
		pNormalisationMap->UnlockRect( D3DCUBEMAP_FACE_NEGATIVE_X, 0 );
	}
	else
		ERROR_MSG( "EffectVisualContext::createNormalisationMap - couldn't get cube surface.  error code %lx\n", hr );

	//positive x
	hr = pNormalisationMap->LockRect( D3DCUBEMAP_FACE_POSITIVE_Y, 0, &lockedRect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		fillNormMapSurface( (uint32*)lockedRect.pBits, Vector3( 1, 0, 0 ), Vector3( 0, 0,  1), Vector3( 0,  1, 0 )  );
		pNormalisationMap->UnlockRect( D3DCUBEMAP_FACE_POSITIVE_Y, 0 );
	}
	else
		ERROR_MSG( "EffectVisualContext::createNormalisationMap - couldn't get cube surface.  error code %lx\n", hr );

	//positive x
	hr = pNormalisationMap->LockRect( D3DCUBEMAP_FACE_NEGATIVE_Y, 0, &lockedRect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		fillNormMapSurface( (uint32*)lockedRect.pBits, Vector3( 1, 0, 0 ), Vector3( 0, 0, -1), Vector3( 0, -1, 0 )  );
		pNormalisationMap->UnlockRect( D3DCUBEMAP_FACE_NEGATIVE_Y, 0 );
	}
	else
		ERROR_MSG( "EffectVisualContext::createNormalisationMap - couldn't get cube surface.  error code %lx\n", hr );

	//positive x
	hr = pNormalisationMap->LockRect( D3DCUBEMAP_FACE_POSITIVE_Z, 0, &lockedRect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		fillNormMapSurface( (uint32*)lockedRect.pBits, Vector3(  1, 0, 0 ), Vector3( 0, -1, 0), Vector3( 0, 0,  1 )  );
		pNormalisationMap->UnlockRect( D3DCUBEMAP_FACE_POSITIVE_Z, 0 );
	}
	else
		ERROR_MSG( "EffectVisualContext::createNormalisationMap - couldn't get cube surface.  error code %lx\n", hr );

	//positive x
	hr = pNormalisationMap->LockRect( D3DCUBEMAP_FACE_NEGATIVE_Z, 0, &lockedRect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		fillNormMapSurface( (uint32*)lockedRect.pBits, Vector3( -1, 0, 0 ), Vector3( 0, -1, 0), Vector3( 0, 0, -1 )  );
		pNormalisationMap->UnlockRect( D3DCUBEMAP_FACE_NEGATIVE_Z, 0 );
	}
	else
		ERROR_MSG( "EffectVisualContext::createNormalisationMap - couldn't get cube surface.  error code %lx\n", hr );

	pNormalisationMap_ = pNormalisationMap;
	pNormalisationMap->Release();
}



}	// namespace Moo

// effect_visual_context.cpp
