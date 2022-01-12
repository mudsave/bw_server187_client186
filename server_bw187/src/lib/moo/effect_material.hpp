/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EFFECT_MATERIAL_HPP
#define EFFECT_MATERIAL_HPP

#include "moo_dx.hpp"
#include "managed_effect.hpp"
#include "effect_state_manager.hpp"
#include "physics2/worldtri.hpp"

namespace Moo
{

class EffectProperty;
typedef SmartPointer<EffectProperty> EffectPropertyPtr;
class ManagedEffect;
typedef SmartPointer<ManagedEffect> ManagedEffectPtr;

class VisualChannel;
class EffectMaterial;
typedef SmartPointer<EffectMaterial> EffectMaterialPtr;

/**
 * This class implements the basic material used to render visuals.
 * The material uses a d3dx effect as it's basis for rendering operations.
 */
class EffectMaterial :
	private EffectTechniqueSetting::IListener,
	private EffectManager::IListener,

	public SafeReferenceCount
{
public:
	EffectMaterial();
	explicit EffectMaterial( const EffectMaterial & other );

	~EffectMaterial();

	bool load( DataSectionPtr pSection, bool addDefault = true );

	bool initFromEffect( const std::string& effect, const std::string& diffuseMap = "", int doubleSided = -1 );

	bool begin() const;
	bool end() const;

	bool beginPass( uint32 pass ) const;
	bool endPass() const;

	bool commitChanges() const;

	StateRecorder* recordPass( uint32 pass ) const;
	uint32 nPasses() const
	{
		UINT nPasses = 0;
		for (uint32 i = 0; i < nPasses_.size(); i++)
		{
			nPasses = max( nPasses, nPasses_[i] );
		}
		return nPasses;
	}

	VisualChannel* channel() const
	{
		if (!initComplete_)
		{
			finishInit();
		}
		return channel_;
	}
	void channel( VisualChannel* channel ) { channel_ = channel; }

	typedef std::map< D3DXHANDLE, EffectPropertyPtr > Properties;

	const std::string& identifier() const { return identifier_; }
	void identifier( const std::string& identifier ) { identifier_ = identifier; }

	D3DXHANDLE hTechnique( uint32 effectIndex = 0) const { return hTechniques_[effectIndex]; };
	bool hTechnique( D3DXHANDLE hTec, uint32 effectIndex = 0 );

	ManagedEffectPtr pEffect( uint32 effectIndex = 0 ) { return pEffects_[effectIndex].first; }
    uint32 nEffects() const { return pEffects_.size(); }

	bool skinned() const { return skinned_; }
	bool bumpMapped() const { return bumpMapped_; }

	EffectMaterial & operator=( const EffectMaterial & other );

	bool boolProperty( bool & result, const std::string & name ) const;
	bool intProperty( int & result, const std::string & name ) const;
	bool floatProperty( float & result, const std::string & name ) const;
	bool vectorProperty( Vector4 & result, const std::string & name ) const;
	EffectPropertyPtr getProperty( const std::string & name );
	ConstEffectPropertyPtr getProperty( const std::string & name ) const;
	bool replaceProperty( const std::string & name, EffectPropertyPtr effectProperty );

	WorldTriangle::Flags getFlags( int objectMaterialKind ) const
	{
		return (materialKind_ != 0) ?
			WorldTriangle::packFlags( collisionFlags_, materialKind_ ) :
			WorldTriangle::packFlags( collisionFlags_, objectMaterialKind );
	}

	static void finishEffectInits();

private:
	bool hTechniqueInternal( D3DXHANDLE hTec, uint32 effectIndex = 0 );
	void finishInit() const;

	bool getFirstValidTechnique(ManagedEffectPtr effect, D3DXHANDLE & hT) const;
	bool validateShaderVersion(ID3DXEffect * effect, D3DXHANDLE hTechnique) const;

	bool loadInternal( DataSectionPtr pSection, EffectPropertyMappings& outProperties );
	bool loadEffect( const std::string& effectResource );

	virtual void onSelectTechnique(ManagedEffect * effect, D3DXHANDLE hTec);
	virtual void onSelectPSVersionCap(int psVerCap);

	mutable std::vector<UINT> nPasses_;
	std::vector<D3DXHANDLE>	hTechniques_;
	typedef std::pair<ManagedEffectPtr, int> EffectIntPair;
	std::vector<EffectIntPair> pEffects_;
	std::vector<Properties> properties_;

	std::string identifier_;

	VisualChannel*	channel_;
	bool			channelOverridden_;
	bool			skinned_;
	bool			bumpMapped_;
	mutable bool	initComplete_;
	bool			techniqueExplicitlySet_;

	static SimpleMutex effectInitQueueMutex;
	static std::list<EffectMaterial*> effectInitQueue;
	
public:
	Properties & properties(uint32 effectIndex = 0)	{ return properties_[effectIndex]; }

public:
#ifdef EDITOR_ENABLED
	bool isDefault(EffectPropertyPtr pProp, uint32 effectIndex);
	void replaceDefaults();
	bool bspModified_;
#endif

	int	materialKind_;
	int collisionFlags_;
};

}

#endif // EFFECT_MATERIAL_HPP
