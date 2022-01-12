/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MANAGED_EFFECT_HPP
#define MANAGED_EFFECT_HPP

#include "moo_dx.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/concurrency.hpp"
#include "math/vector4.hpp"
#include "graphics_settings.hpp"
#include "effect_constant_value.hpp"
#include "device_callback.hpp"
#include "com_object_wrap.hpp"
#include "base_texture.hpp"
#include "graphics_settings.hpp"

class BinaryBlock;
typedef SmartPointer<BinaryBlock> BinaryPtr;

namespace Moo
{

// Forward declarations
class EffectTechniqueSetting;
class EffectMacroSetting;


/*
 * This class is the base class for the effect properties.
 */
class EffectProperty : public ReferenceCount
{
public:
	virtual bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty ) = 0;
	virtual void be( const Vector4 & v ) { }
	virtual void be( const BaseTexturePtr pTex ) {}
	virtual bool getBool( bool & b ) const { return false; };
	virtual bool getInt( int & i ) const { return false; };
	virtual bool getFloat( float & f ) const { return false; };
	virtual bool getVector( Vector4 & v ) const { return false; };
	virtual bool getResourceID( std::string & s ) const { return false; };
	virtual void attach( D3DXHANDLE hProperty, ID3DXEffect* pEffect ){};
private:
};

typedef SmartPointer<EffectProperty> EffectPropertyPtr;
typedef ConstSmartPointer<EffectProperty> ConstEffectPropertyPtr;
typedef std::map< std::string, EffectPropertyPtr> EffectPropertyMappings;

/**
 *	This class manages an individual d3d effect and its standard properties.
 */
class ManagedEffect : public ReferenceCount
{
public:
	ManagedEffect();
	~ManagedEffect();

	bool load( const std::string& resourceName );	
	bool registerGraphicsSettings(const std::string & effectResource);

	ID3DXEffect* pEffect() { return pEffect_.pComObject(); }
	const std::string& resourceID() { return resourceID_; }

	EffectPropertyMappings& defaultProperties() { return defaultProperties_; };

	void setAutoConstants();

	typedef std::pair<D3DXHANDLE, EffectConstantValuePtr*> MappedConstant;
	typedef std::vector<MappedConstant> MappedConstants;

	typedef std::vector<RecordedEffectConstantPtr> RecordedEffectConstants;

	void recordAutoConstants( RecordedEffectConstants& recordedList );

	struct TechniqueInfo
	{
		std::string name;
		D3DXHANDLE  handle;
		int         psVersion;
		bool        supported;
	};

	BinaryPtr compile( const std::string& resName, D3DXMACRO * preProcessDefinition=NULL );
	bool cache(BinaryPtr bin, D3DXMACRO * preProcessDefinition=NULL );
	
	EffectTechniqueSetting * graphicsSettingEntry();

	int maxPSVersion(D3DXHANDLE hTechnique);
	static int maxPSVersion(ID3DXEffect * d3dxeffect, D3DXHANDLE hTechnique);
	
private:
	ComObjectWrap<ID3DXEffect> pEffect_;

	EffectPropertyMappings	defaultProperties_;
	MappedConstants			mappedConstants_;
	ManagedEffect( const ManagedEffect& );
	ManagedEffect& operator=( const ManagedEffect& );

	D3DXMACRO * generatePreprocessors();
	
	typedef std::vector<TechniqueInfo> TechniqueInfoVector;
	typedef SmartPointer<EffectTechniqueSetting> EffectTechniqueSettingPtr;
	std::string			resourceID_;
	TechniqueInfoVector techniques_;
	std::string         settingName_;
	EffectTechniqueSettingPtr    settingEntry_;
	bool				settingsAdded_;
};

typedef SmartPointer<ManagedEffect> ManagedEffectPtr;

/**
 *	Used internally to register graphics settings created by annotating effect
 *	files and their techniques. Effect files sometimes provide alternative 
 *	techniques to render the same material. Normally, the techniques to be 
 *	used is selected based on hardware capabilities. By annotating the effect
 *	files with specific variables, you can allow the user to select between
 *	the supported techniques via the graphics settings facility. 
 *
 *	To register an effect file as a graphics setting, you need (1) to add
 *	a top level parameter named "graphicsSetting", with a string annotation 
 *	named "label". This string will be used as the setting label property:
 *
 *	int graphicsSetting
 *	<
 *		string label = "PARALLAX_MAPPING";
 *	>;
 *	
 *	And (2) have, for each selectable technique, a string annotation named
 *	"label". This string will be used as the option entry label property:
 *
 *	technique lighting_shader1
 *	< 
 *		string label = "SHADER_MODEL_1";
 *	>
 *	{ (...) }
 *
 *	technique software_fallback
 *	< 
 *		string label = "SHADER_MODEL_0";
 *	>
 *	{ (...) }
 */
class EffectTechniqueSetting : public GraphicsSetting
{
public:
	typedef GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;

	class IListener
	{
	public:
		virtual void onSelectTechnique(
				ManagedEffect * effect, D3DXHANDLE hTec) = 0;
	};

	EffectTechniqueSetting(ManagedEffect * owner, const std::string & name);
	
	void addTechnique(const ManagedEffect::TechniqueInfo &info);	
	virtual void onOptionSelected(int optionIndex);
	void setPSCapOption(int psVersionCap);
	
	D3DXHANDLE activeTechniqueHandle() const;

	int addListener(IListener * listener);
	void delListener(int listenerId);
	
private:	
	typedef std::pair<D3DXHANDLE, int>        D3DXHandlesPSVerPair;
	typedef std::vector<D3DXHandlesPSVerPair> D3DXHandlesPSVerVector;
	typedef std::map<int, IListener *>        ListenersMap;
	
	ManagedEffect        * owner_;
	D3DXHandlesPSVerVector techniques_;
	ListenersMap           listeners_;
	SimpleMutex	           listenersLock_;

	static int listenersId;
};

/*
 *	Used internally to register settings that work by redefining effect files 
 *	compile time macros. This settings require the client application to be 
 *	restarted form them to come into effect.
 *
 *	Note: EffectMacroSetting instances need to be static to make sure it gets 
 *	instantiated before the EffectManager is initialized). For an example on
 *	how to use EffectMacroSetting, see romp/sky_light_map.cpp.
 */
class EffectMacroSetting : public GraphicsSetting
{
public:
	typedef SmartPointer<EffectMacroSetting> EffectMacroSettingPtr;
	typedef std::vector<EffectMacroSettingPtr> MacroSettingVector;
	typedef void(*SetupFunc)(EffectMacroSetting &);

	EffectMacroSetting(
		const std::string & description, 
		const std::string & Macro, 
		SetupFunc setupFunc);

	void addOption(
		const std::string & description, 
		             bool   isSupported, 
		const std::string & value);
		
	virtual void onOptionSelected(int optionIndex);

	static void setupSettings();
	
private:
	typedef std::vector<std::string> StringVector;
	
	std::string macro_;
	SetupFunc   setupFunc_;
	
	StringVector values_;

	static void add(EffectMacroSettingPtr setting);
	static void updateManagerInfix();
	
	friend class EffectManager;
};


/**
 *	This class loads and manages managed effects.
 */
class EffectManager : public DeviceCallback
{
public:
	typedef std::map< std::string, std::string > StringStringMap;	
	
	static EffectManager* instance();
    ~EffectManager();
	
	static ManagedEffect* get( const std::string& resourceID );
	static void compileOnly( const std::string& resourceID );

	void createUnmanagedObjects();
	void deleteUnmanagedObjects();
	
	typedef std::vector< std::string > IncludePaths;
	const IncludePaths& includePaths() const { return includePaths_; }
	IncludePaths& includePaths() { return includePaths_; }

	void addIncludePaths( const std::string& pathString );

	bool checkModified( const std::string& objectName, const std::string& resName );

	bool registerGraphicsSettings();

	void setMacroDefinition(
		const std::string & key, 
		const std::string & value);
		
	const StringStringMap & macros();
	
	const std::string & fxoInfix() const;
	void fxoInfix(const std::string & infix);
	
	int PSVersionCap() const;
	
	static void init();
	static void fini();
	
	class IListener
	{
	public:
		virtual void onSelectPSVersionCap(int psVerCap) = 0;
	};
	
	void addListener(IListener * listener);
	void delListener(IListener * listener);

private:
	EffectManager();

	ManagedEffect*	find( const std::string& resourceID );
	void			add( ManagedEffect* pEffect, const std::string& resourceID );

	static void del( ManagedEffect* pEffect );
	void		delInternal( ManagedEffect* pEffect );
	
	typedef std::map< std::string, ManagedEffect* > Effects;

	Effects         effects_;
	IncludePaths    includePaths_;
	SimpleMutex	    effectsLock_;
	StringStringMap macros_;	
	std::string		fxoInfix_;
	
	typedef std::vector<IListener *> ListenerVector;
	ListenerVector listeners_;
	SimpleMutex	   listenersLock_;
	
	typedef GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	GraphicsSettingPtr psVerCapSettings_;
	void setPSVerCapOption(int);

	friend ManagedEffect::~ManagedEffect();
	static EffectManager* pInstance_;
};


class EffectPropertyFunctor
{
public:
	virtual EffectPropertyPtr create( DataSectionPtr pSection ) = 0;
	virtual EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect ) = 0;
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc ) = 0;
private:
};

typedef std::map< std::string, EffectPropertyFunctor* > PropertyFunctors;
extern PropertyFunctors g_effectPropertyProcessors;

}

#endif // MANAGED_EFFECT_HPP
