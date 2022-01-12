/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MATERIAL_UTILITY_HPP
#define MATERIAL_UTILITY_HPP

namespace Moo
{
	class EffectMaterial;
};

#include "moo/com_object_wrap.hpp"

class MaterialUtility
{
public:
	static ComObjectWrap<ID3DXEffect> MaterialUtility::effect(
		Moo::EffectMaterial* material, int effectIndex = 0 );

	/**
	 *	Technique helpers.
	 */
	static int numTechniques( Moo::EffectMaterial* material );

	static int listTechniques(
		Moo::EffectMaterial* m,
		std::vector<std::string>& retVector );

	static bool MaterialUtility::isTechniqueValid(
		Moo::EffectMaterial* m,
		int index );

	static bool MaterialUtility::viewTechnique(
		Moo::EffectMaterial* m,
		int index );

	static bool MaterialUtility::viewTechnique(
		Moo::EffectMaterial* m,
		const std::string& name );

	static int MaterialUtility::techniqueByName(
		Moo::EffectMaterial* material,
		const std::string& name );

	static int currentTechnique( Moo::EffectMaterial* material );

	/**
	 *	Tweakable Property helpers.
	 */
	static int numProperties( Moo::EffectMaterial* material );

	static int listProperties(
		Moo::EffectMaterial* m,
		std::vector<std::string>& retVector );

    static bool artistEditable( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
	static bool worldBuilderEditable( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
    static std::string UIName( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
	static std::string UIDesc( ID3DXEffect* pEffect, D3DXHANDLE hProperty );
	static std::string UIWidget( ID3DXEffect* pEffect, D3DXHANDLE hProperty );

	static void MaterialUtility::setTexture( Moo::EffectMaterial* material,
		int idx, const std::string& textureName );

	/**
	 *	Save a material to a data section.  Note that
	 *	materialProperties / runtimeInitMaterialProperties must have been
	 *	called during application startup in order for the save to work.
	 */
	static void save(
		Moo::EffectMaterial* m,
		DataSectionPtr pSection,
		bool worldBuilderEditableOnly = false );
};

#endif