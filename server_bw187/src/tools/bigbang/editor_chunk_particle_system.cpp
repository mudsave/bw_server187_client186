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
#include "editor_chunk_particle_system.hpp"
#include "editor_chunk_substance.ipp"

#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "romp/super_model.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunks/editor_chunk.hpp"
#include "gizmo/undoredo.hpp"

#include "resmgr/auto_config.hpp"
#include "particle/meta_particle_system.hpp"
#include "common/tools_common.hpp"
#include <algorithm>

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )


static AutoConfigString s_notFoundModel( "system/notFoundModel" );	


std::map<std::string, std::set<EditorChunkParticleSystem*> > EditorChunkParticleSystem::editorChunkParticleSystem_;

void EditorChunkParticleSystem::add( EditorChunkParticleSystem* system, const std::string& filename )
{
	editorChunkParticleSystem_[ filename ].insert( system );
}

void EditorChunkParticleSystem::remove( EditorChunkParticleSystem* system )
{
	for( std::map<std::string, std::set<EditorChunkParticleSystem*> >::iterator iter =
		editorChunkParticleSystem_.begin(); iter != editorChunkParticleSystem_.end(); ++iter )
	{
		std::set<EditorChunkParticleSystem*>& particleSet = iter->second;
		for( std::set<EditorChunkParticleSystem*>::iterator siter = particleSet.begin();
			siter != particleSet.end(); ++siter )
		{
			if( *siter == system )
			{
				particleSet.erase( siter );
				if( particleSet.size() )
					editorChunkParticleSystem_.erase( iter );
				return;
			}
		}
	}
}

void EditorChunkParticleSystem::reload( const std::string& filename )
{
	BWResource::instance().purgeAll();
	std::set<EditorChunkParticleSystem*> particleSet =
		editorChunkParticleSystem_[ BWResource::dissolveFilename( filename ) ];
	for( std::set<EditorChunkParticleSystem*>::iterator iter = particleSet.begin();
		iter != particleSet.end(); ++iter )
	{
		Chunk* myChunk = (*iter)->chunk();
		(*iter)->toss( NULL );
		(*iter)->load( BWResource::dissolveFilename( filename ) );
		(*iter)->toss( myChunk );
	}
}
// -----------------------------------------------------------------------------
// Section: EditorChunkParticleSystem
// -----------------------------------------------------------------------------
uint32 EditorChunkParticleSystem::s_settingsMark_ = -16;
/**
 *	Constructor.
 */
EditorChunkParticleSystem::EditorChunkParticleSystem()
{
}


/**
 *	Destructor.
 */
EditorChunkParticleSystem::~EditorChunkParticleSystem()
{
	remove( this );
}

bool EditorChunkParticleSystem::edShouldDraw()
{
	if( !EditorChunkSubstance<ChunkParticles>::edShouldDraw() )
		return false;
	static int renderParticle = 1;

	if (Moo::rc().frameTimestamp() != s_settingsMark_)
		renderParticle = Options::getOptionInt( "render/scenery/particle", renderParticle );

	return renderParticle != 0;
}

void EditorChunkParticleSystem::draw()
{
	if (!edShouldDraw())
		return;

	if( BigBang::instance().drawSelection() )
		Moo::rc().setRenderState( D3DRS_TEXTUREFACTOR, (DWORD)this );

	Moo::rc().push();
	Moo::rc().preMultiply( this->edTransform() );

	ModelPtr mp = this->reprModel();
	mp->dress();	// should really be using a supermodel...
	mp->draw( true );

	Moo::rc().pop();

	if ( system_ == NULL )
		return;

	if( !BigBang::instance().drawSelection() )
	{
		ChunkParticles::draw();

		ChunkParticles::drawBoundingBoxes(
			BoundingBox::s_insideOut_,
			BoundingBox::s_insideOut_,
			Matrix::identity );
	}
}

/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkParticleSystem::load( DataSectionPtr pSection, Chunk* chunk, std::string* errorString )
{
	remove( this );
	bool ok = this->EditorChunkSubstance<ChunkParticles>::load( pSection );
	resourceName_ = pSection->readString( "resource" );
	add( this, resourceName_ );
	if ( !ok )
	{
		pOriginalSect_ = pSection;
		BigBang::instance().addError(
			chunk, this, "Couldn't load particle system: %s",
			pSection->readString( "resource" ).c_str() );
	}

	return true;
}


/**
 *	Save any property changes to this data section
 */
bool EditorChunkParticleSystem::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	if ( system_ == NULL )
	{
		// the particle system didn't load, so just save the original section
		// but set the appropriate transform.
		pSection->copy( pOriginalSect_ );
		pSection->writeMatrix34( "transform", localTransform_ );
		return true;
	}

	pSection->writeString( "resource", resourceName_ );
	pSection->writeMatrix34( "transform", localTransform_ );
	pSection->writeBool( "reflectionVisible", getReflectionVis() );

	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkParticleSystem::edTransform()
{
	return localTransform_;
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkParticleSystem::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		// move the system
		localTransform_ = m;
		Matrix world;
		world.multiply( m, chunk()->transform() );
		setMatrix( world );

		// move the bounding boxes too
		if ( system_ != NULL )
			system_->clear();

		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;
	
	// clear to reset bounding boxes (otherwise will be stretched during undo/redo)
	if ( system_ != NULL )
		system_->clear();
	
	// ok, accept the transform change then
	localTransform_.multiply( m, pOldChunk->transform() );
	localTransform_.postMultiply( pNewChunk->transformInverse() );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	// check to see if we are undoing or redoing so we will reset the source action
	if ( system_ != NULL && UndoRedo::instance().isUndoing() )
		system_->setFirstUpdate();
		
	return true;
}

/**
 *	Add the properties of this flare to the given editor
 */
bool EditorChunkParticleSystem::edEdit( class ChunkItemEditor & editor )
{
	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );
	editor.addProperty( new GenRotationProperty( "rotation", pMP ) );
	editor.addProperty( new StaticTextProperty( "particleName", new ConstantDataProxy<StringProxy>(
		resourceName_ ) ) );
	editor.addProperty( new GenBoolProperty( "reflection visible",
		new AccessorDataProxy< EditorChunkParticleSystem, BoolProxy >(
			this, "reflectionVisible",
			&EditorChunkParticleSystem::getReflectionVis,
			&EditorChunkParticleSystem::setReflectionVis ) ) );

	return true;
}

std::vector<std::string> EditorChunkParticleSystem::edCommand( const std::string& path ) const
{
	if ( system_ == NULL )
		return std::vector<std::string>();

	std::vector<std::string> commands;
	commands.push_back( "Edit in Particle Editor..." );
	return commands;
}

bool EditorChunkParticleSystem::edExecuteCommand( const std::string& path, std::vector<std::string>::size_type index )
{
	if ( system_ == NULL )
		return true;

	if( path.empty() && index == 0 )
	{
		char exe[ 1024 ];
		GetModuleFileName( NULL, exe, sizeof( exe ) );
		if( std::count( exe, exe + strlen( exe ), '\\' ) > 2 )
		{
			*strrchr( exe, '\\' ) = 0;
			*strrchr( exe, '\\' ) = 0;
			std::string path = exe;
			path += "\\particleeditor\\";

			if( ToolsCommon::isEval() )
				strcat( exe, "\\particleeditor\\particleeditor_eval.exe" );
			else
				strcat( exe, "\\particleeditor\\particleeditor.exe" );

			std::string commandLine = exe;
			commandLine += " -o";
			commandLine += BWResource::resolveFilename( resourceName_ );
			std::replace( commandLine.begin(), commandLine.end(), '/', '\\' );

			commandLine += ' ';
			commandLine += BWResource::getPathAsCommandLine();

			PROCESS_INFORMATION pi;
			STARTUPINFO si;
			GetStartupInfo( &si );

			if( CreateProcess( exe, (LPSTR)commandLine.c_str(), NULL, NULL, FALSE, 0, NULL, path.c_str(),
				&si, &pi ) )
			{
				CloseHandle( pi.hThread );
				CloseHandle( pi.hProcess );
			}
		}
		return true;
	}
	return false;
}

void EditorChunkParticleSystem::drawBoundingBoxes( const BoundingBox &bb, const BoundingBox &vbb, const Matrix &spaceTrans ) const
{
	if ( system_ == NULL )
		return;

	if (!BigBang::instance().drawSelection())
		EditorChunkSubstance<ChunkParticles>::drawBoundingBoxes( bb, vbb, spaceTrans );
}

/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkParticleSystem::reprModel() const
{
	static ModelPtr psModel =
		Model::get( "resources/models/particle.model" );
	static ModelPtr psBadModel =
		Model::get( s_notFoundModel.value() );
	return system_ == NULL ? psBadModel : psModel;
}


/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkParticleSystem, particles, 1 )
