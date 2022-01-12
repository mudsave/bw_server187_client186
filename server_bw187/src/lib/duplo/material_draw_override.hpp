/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MATERIAL_DRAW_OVERRIDE_HPP
#define MATERIAL_DRAW_OVERRIDE_HPP

#include "moo/render_context.hpp"
#include "moo/visual.hpp"
#include "moo/effect_material.hpp"

class MaterialDrawOverride : public Moo::Visual::DrawOverride
{
public:
	MaterialDrawOverride( const std::string& shaderPrefix, bool allowSorted, int doubleSided = -1 );
	HRESULT draw( Moo::Visual* pVisual, bool ignoreBoundingBox = false );

	bool allowSorted_;
	Moo::EffectMaterial pSkinnedEffect_;
	Moo::EffectMaterial pHardSkinnedEffect_;
	Moo::EffectMaterial pRigidEffect_;
};

#endif