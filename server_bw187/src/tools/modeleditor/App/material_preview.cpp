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

#include "romp/geometrics.hpp"

#include "page_materials.hpp"

#include "material_preview.hpp"

DECLARE_DEBUG_COMPONENT2( "MaterialPreview", 0 )

void MaterialPreview::update()
{
	if ((!needsUpdate_) || (updating_) ||
		(PageMaterials::currPage() == NULL) ||
		(PageMaterials::currPage()->currMaterial() == NULL))
			return;

	needsUpdate_ = false;

	if (!rt_)
	{
		rt_ = new Moo::RenderTarget( "Material Preview" );
		if ((!rt_) || (!rt_->create( 128,128 )))
		{
			WARNING_MSG( "Could not create render target for material preview.\n" );
			rt_ = NULL;
			return;
		}
	}

	updating_ = true;

	if ( rt_->push() )
	{
		Moo::rc().beginScene();
	
		Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			RGB( 255, 255, 255), 1, 0 );
		
		Geometrics::drawRect(
			Vector2( 0, 0 ),
			Vector2( 128, 128 ),
			*(PageMaterials::currPage()->currMaterial())
		);

		Moo::rc().endScene();

		std::string fileName = BWResource::resolveFilename( "resources/material_preview.bmp" );

		HRESULT hr = D3DXSaveTextureToFile( fileName.c_str(), D3DXIFF_BMP, rt_->pTexture(), NULL );

		rt_->pop();

		if ( FAILED( hr ) )
		{
			WARNING_MSG( "Could not create material preview file (D3D error = 0x%x).\n", hr);
		}
	}

	hasNew_ = true;
	updating_ = false;
	
}