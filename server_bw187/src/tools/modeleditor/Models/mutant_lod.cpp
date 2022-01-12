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

#include "romp/super_model.hpp"

#include "resmgr/xml_section.hpp"

#include "undo_redo.hpp"

#include "romp/geometrics.hpp"

#include "utilities.hpp"

#include "page_lod_consts.hpp"

#include "mutant.hpp"

DECLARE_DEBUG_COMPONENT2( "Mutant_LOD", 0 )

float Mutant::lodExtent( const std::string& modelFile )
{
	//First make sure the model exists
	if (models_.find(modelFile) == models_.end())
		return LOD_HIDDEN;

	return models_[modelFile]->readFloat( "extent", LOD_HIDDEN );
}

void Mutant::lodExtent( const std::string& modelFile, float extent )
{
	//First make sure the model exists
	if (models_.find(modelFile) == models_.end())
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, models_[modelFile] ));

	if (extent != LOD_HIDDEN)
	{
		models_[modelFile]->writeFloat( "extent", extent );
	}
	else
	{
		models_[modelFile]->delChild( "extent" );
	}
	
	dirty( models_[modelFile] );
}

void Mutant::lodParents( std::string modelName, std::vector< std::string >& parents )
{
	DataSectionPtr model = BWResource::openSection( modelName, false );
	while ( model )
	{
		parents.push_back( modelName );
		modelName = model->readString( "parent", "" ) + ".model";
		model = BWResource::openSection( modelName, false );
	}
}

bool Mutant::hasParent( const std::string& modelName )
{
	return (models_.find(modelName) != models_.end());
}

std::string Mutant::lodParent( const std::string& modelFile )
{
	//First make sure the model exists
	if (models_.find(modelFile) == models_.end())
		return "";

	return models_[modelFile]->readString( "parent", "" );
}

void Mutant::lodParent( const std::string& modelFile, const std::string& parent )
{
	//First make sure the model exists
	if (models_.find(modelFile) == models_.end())
		return;

	UndoRedo::instance().add( new UndoRedoOp( 0, models_[modelFile] ));

	if (parent != "")
	{
		models_[modelFile]->writeString( "parent", parent );
	}
	else
	{
		models_[modelFile]->delChild( "parent" );
		models_[modelFile]->delChild( "extent" );
	}
	
	dirty( models_[modelFile] );
}

/*
 * This method commits a LOD list that has been edited by using the lod bar
 */
void Mutant::lodList( LODList* newList )
{
	//Update all the extents
	for (unsigned i=0; i<newList->size(); i++)
	{
		lodExtent((*newList)[i].first.second, (*newList)[i].second );
	}

	reloadAllLists();

}

/*
 * This method sets the virtual LOD distance
 */
void Mutant::virtualDist( float dist )
{
	virtualDist_ = dist;
}
