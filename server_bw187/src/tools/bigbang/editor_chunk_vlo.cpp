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

#include "editor_chunk_vlo.hpp"
#include "editor_chunk_substance.ipp"

#include "chunk_item_placer.hpp"
#include "chunk/unique_id.hpp"
#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "romp/super_model.hpp"

#include "chunk/chunk.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_vlo_obstacle.hpp"

#include "chunks/editor_chunk.hpp"


// -----------------------------------------------------------------------------
// Section: EditorChunkVLO
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
EditorChunkVLO::EditorChunkVLO( std::string type ) :
	type_( type ),
	colliding_( false )
{
	s_drawAlways_ = false;	// turn off its draw flag thankyouverymuch	
	drawTransient_ = false;
}


/**
 *	Constructor.
 */
EditorChunkVLO::EditorChunkVLO( ) :
	colliding_( false )
{
	s_drawAlways_ = false;	// turn off its draw flag thankyouverymuch	
	drawTransient_ = false;
}


/**
 *	Destructor.
 */
EditorChunkVLO::~EditorChunkVLO()
{
}


void EditorChunkVLO::edPostCreate()
{	
	if (pObject_)
	{
		pObject_->deleted(false);
		objectCreated();
	}
}


/**
 * 
 */
void VeryLargeObject::edDelete( ChunkVLO* instigator )
{
	deleted_ = true;
	dirtyReferences();
	destroyDirtyRefs( instigator );

	if (instigator && instigator->chunk())
		chunkPath_ = instigator->chunk()->mapping()->path();
}


/**
 * Places a VLO reference into the specified chunk.
 */
ChunkVLO* VeryLargeObject::createReference( std::string& uid, Chunk * pChunk )
{
	EditorChunkVLO * pItem = new EditorChunkVLO( type_ );
	if (pItem->load( uid, pChunk ))
	{
		pChunk->addStaticItem( pItem );
		BigBang::instance().changedChunk( pChunk );
		return pItem;
	}
	delete pItem;
	return NULL;
}


/**
 * Remove any references not being used. 
 */
void VeryLargeObject::destroyDirtyRefs( ChunkVLO *instigator )
{
	// go through the reference list and cull those marked dirty
	ChunkItemList::iterator it;
	for (it = itemList_.begin(); it != itemList_.end();)
	{
 		if ((*it)->dirtyRef() && ((*it) != instigator))
		{
			if ((*it)->chunk())
			{
				if ((*it)->root())
				{
					UndoRedo::instance().add(
						new ChunkItemExistenceOperation( (*it), (*it)->chunk() ) );
				}

				BigBang::instance().changedChunk((*it)->chunk());
				(*it)->chunk()->delStaticItem( (*it) );
			}
		}
		if (listModified_)
		{
			it = itemList_.begin();
			listModified_=false;
		}
		else
		{
			++it;
		}
	}
}


/**
 * Load the reference
 *
 */
bool EditorChunkVLO::load( std::string& uid, Chunk * pChunk )
{
	pObject_ = VeryLargeObject::getObject( uid );
	if (pObject_)
	{	
		uid_ = uid;
		updateTransform( pChunk );
		return true;
	}	
	return false;
}


/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkVLO::load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	localPos_ = Vector3(50,0,50); //middle it
	edTransform(); //initialise the transform
	vloTransform_ = Matrix::identity;
	vloTransform_.translation( -localPos_ );
	vloTransform_.postMultiply( pChunk->transformInverse() );
	if (this->EditorChunkSubstance<ChunkVLO>::load( pSection, pChunk ))
	{		
		type_ = pSection->readString( "type", "" );
		uid_ = pSection->readString( "uid", "" );

		vloTransform_.postMultiply( pObject_->origin() );

		pObject_->addItem( this );
		return true;
	}
	else if (createVLO( pSection, pChunk ))
	{
		type_ = pSection->readString( "type", "" );
		uid_ = pObject_->getUID();

		vloTransform_.postMultiply( pObject_->origin() );

		pObject_->addItem( this );
		return true;
	}
	else if ( pSection->readString( "uid", "" ) != "" )
	{
		EditorChunkCache::instance( *pChunk ).addInvalidSection( pSection );			
		BigBang::instance().changedChunk(pChunk);
	}

	if( errorString )
	{
		*errorString = "Failed to load " + pSection->readString( "type", "<unknown type>" ) +
			" VLO " + pSection->readString( "uid", "<unknown id>" );
	}
	return false;
}

bool EditorChunkVLO::legacyLoad( DataSectionPtr pSection, Chunk * pChunk, std::string& type )
{
	localPos_ = Vector3(50,0,50); //middle it
	edTransform(); //initialise the transform
	vloTransform_ = Matrix::identity;
	vloTransform_.translation( -localPos_ );
	vloTransform_.postMultiply( pChunk->transformInverse() );


	//pSection->setParent(NULL);
	BigBang::instance().changedChunk( pChunk );

	if (createLegacyVLO( pSection, pChunk, type ))
	{
		Matrix m( pObject_->edTransform() );
		m.postMultiply( pChunk->transform() );
		pObject_->updateLocalVars( m );

		//edTransform(m,false);

		type_ = type;
		uid_ = pObject_->getUID();

		vloTransform_.postMultiply( pObject_->origin() );

		pObject_->addItem( this );
		return true;
	}
	return false;
}


void EditorChunkSubstance<ChunkVLO>::toss( Chunk * pChunk )
{
	if (pChunk_ != NULL)
	{
		removeCollisionScene();

		if (pOwnSect_)
		{
			EditorChunkCache::instance( *pChunk_ ).
				pChunkSection()->delChild( pOwnSect_ );
			pOwnSect_ = NULL;
		}
	}

	this->ChunkVLO::toss( pChunk );

	if (pChunk_ != NULL)
	{
		///pObject_->addItem( this );
		//if (!pOwnSect_)
		if (!pOwnSect_ && pChunk_->loaded())
		{
			pOwnSect_ = EditorChunkCache::instance( *pChunk_ ).
				pChunkSection()->newSection( this->sectName() );
			this->edSave( pOwnSect_ );
		}
	}
}


/**
 *
 *
 */
void EditorChunkVLO::edPreDelete()
{
	if (pObject_)
	{
		pObject_->edDelete( this );
		//pObject_->removeItem( this );
		//pObject_ = NULL;
	}
}


/**
 *	This method does extra stuff when this item is tossed between chunks.
 *
 *	It makes sure the world variables used to create the water get updated.
 */
void EditorChunkVLO::toss( Chunk * pChunk )
{
	// get the template class to do the real work
	this->EditorChunkSubstance<ChunkVLO>::toss( pChunk );

	// and update our world vars if we're in a chunk
	if (pChunk_ != NULL)
	{
		if( BigBang::instance().isDirtyVlo( pChunk, pObject_->getUID() ) )
			touch();
		pObject_->addItem( this );

		this->updateWorldVars( pChunk_->transform() );
		if (pObject_)
		{
			ChunkVLO* focus = pObject_->containsChunk( pChunk_ );
			if ( focus && focus != this)
				focus->dirtyRef(true);

			pObject_->updateReferences( pChunk_ );
		}
	}
}


/**
 *
 *
 */
void EditorChunkSubstance<ChunkVLO>::addAsObstacle()
{
	Matrix world( pChunk_->transform() );
	world.preMultiply( this->edTransform() );
	ChunkVLOObstacle::instance( *pChunk_ ).addModel(
		this->reprModel(), world, this );
}


/**
 *	This is called after water has finished with the collision scene,
 *	i.e. we can now add stuff that it would otherwise collide with
 */
void EditorChunkVLO::objectCreated()
{
	if (!colliding_)
	{
		this->EditorChunkSubstance<ChunkVLO>::addAsObstacle();
		colliding_ = true;
	}
}


/**
 *	Additional save
 */
void EditorChunkVLO::edChunkSave( )
{
	if (pObject_)
		pObject_->saveFile(chunk());
}


/**
 *	Save any property changes to this data section
 */
bool EditorChunkVLO::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	pSection->writeString( "uid", uid_ );
	pSection->writeString( "type", type_ );

	pObject_->save();	
	return true;
}

class VLOCloneNotifier : CloneNotifier
{
	std::map<UniqueID,UniqueID> idMap_;
	std::set<DataSectionPtr> sects_;
	void begin()
	{
		idMap_.clear();
		sects_.clear();
	}
	void end()
	{
		idMap_.clear();
		sects_.clear();
	}
public:
	UniqueID get( UniqueID id )
	{
		if( !in( id ) )
			idMap_[ id ] = UniqueID::generate();
		return idMap_[ id ];
	}
	bool in( UniqueID id ) const
	{
		return idMap_.find( id ) != idMap_.end();
	}
	void add( DataSectionPtr sect )
	{
		sects_.insert( sect );
	}
}
VLOCloneNotifier;

/**
 * get the DataSection for clone
 */
void EditorChunkVLO::edCloneSection( Chunk* destChunk, const Matrix& destMatrixInChunk, DataSectionPtr destDS )
{
	ChunkVLO::edCloneSection( destChunk, destMatrixInChunk, destDS );
	if( type_ == "water" )
	{
		if( !VLOCloneNotifier.in( uid_ ) )
		{
			std::string uid = VLOCloneNotifier.get( uid_ );
			strlwr( &uid[0] );
			DataSectionPtr newDS = BWResource::openSection( chunk()->mapping()->path() + '/' + uid + ".vlo", true );
			newDS->copy( pObject_->section() );
			Matrix m = destMatrixInChunk;
			m.postMultiply( destChunk->transform() );
			newDS->writeVector3( "water/position", m.applyToOrigin() );
			VLOCloneNotifier.add( newDS );
		}
		std::string uid = VLOCloneNotifier.get( uid_ );
		strlwr( &uid[0] );
		destDS->writeString( "uid", uid );
	}
}

/**
 * refine the DataSection for chunk clone
 */
bool EditorChunkVLO::edPreChunkClone( Chunk* srcChunk, const Matrix& destChunkMatrix, DataSectionPtr chunkDS )
{
	if( type_ == "water" )
	{
		if( edBelongToChunk() )
		{
			std::vector<DataSectionPtr> vlos;
			chunkDS->openSections( "vlo", vlos );

			bool in = VLOCloneNotifier.in( uid_ );

			std::string uid = VLOCloneNotifier.get( uid_ );
			strlwr( &uid[0] );

			for( std::vector<DataSectionPtr>::iterator iter = vlos.begin();
				iter != vlos.end(); ++iter )
			{
				if( (*iter)->readString( "uid" ) == uid_ )
				{
					(*iter)->writeString( "uid", uid );
				}
			}
			if( !in )
			{
				DataSectionPtr newDS = BWResource::openSection( chunk()->mapping()->path() + '/' + uid + ".vlo", true );
				newDS->copy( pObject_->section() );
				Matrix m = edTransform();
				m.postMultiply( destChunkMatrix );
				newDS->writeVector3( "water/position", m.applyToOrigin() );
				VLOCloneNotifier.add( newDS );
			}
		}
		else
		{
			std::vector<DataSectionPtr> vlos;
			chunkDS->openSections( "vlo", vlos );

			for( std::vector<DataSectionPtr>::iterator iter = vlos.begin();
				iter != vlos.end(); ++iter )
			{
				if( (*iter)->readString( "uid" ) == uid_ )
					chunkDS->delChild( *iter );
			}
		}
	}
	return true;
}

/**
 *	return whether this item belongs to the chunk
 */
bool EditorChunkVLO::edBelongToChunk()
{
	BoundingBox bb = object()->boundingBox();
	BoundingBox chunkBB = chunk()->boundingBox();
	return chunkBB.intersects( bb.minBounds() ) && chunkBB.intersects( bb.maxBounds() );
}

/**
 *	Get the current transform
 */
const Matrix & EditorChunkVLO::edTransform()
{
	if (pObject_ && pChunk_)
		transform_ = pObject_->localTransform( pChunk_ );

	return transform_;
}

/**
 *	mark the VLO dirty
 */
void EditorChunkVLO::touch()
{
	if (pObject_)
		pObject_->dirty();
}

/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkVLO::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );	
	if (pNewChunk == NULL) return false;

	drawTransient_=transient;
	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		this->updateLocalVars( transform_ , pChunk_ );
		this->updateWorldVars( pNewChunk->transform() );
		return true;
	}

	touch();

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );
	this->updateLocalVars( transform_, pNewChunk );

	pObject_->dirtyReferences();

	dirtyRef(false); //make sure we dont delete ourself	

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );
	pObject_->destroyDirtyRefs();

	BigBang::instance().dirtyVLO( type_, uid_, pObject_->boundingBox() );
	//NOTE: world vars will get updated when we are tossed into the new chunk
	return true;
}


/**
 *	Add the properties in the given editor
 */
bool EditorChunkVLO::edEdit( class ChunkItemEditor & editor )
{
	if (pObject_)
		return pObject_->edEdit( editor, this );
	return true;
}


/**
 *	
 */
void EditorChunkVLO::removeCollisionScene()
{
	if (pChunk_)
	{
		ChunkVLOObstacle::instance( *pChunk_ ).delObstacles( this );
		colliding_ = false;
	}
}


/**
 *	
 */
void EditorChunkVLO::updateTransform( Chunk * pChunk )
{
	if (pObject_)
	{
		localPos_ = Vector3(50,0,50); //TODO: un-hardcode the chunk size stuff
		vloTransform_ = Matrix::identity;
		vloTransform_.translation( -localPos_ );
		vloTransform_.postMultiply( pChunk->transformInverse() );		
		vloTransform_.postMultiply( pObject_->origin() );
	}
}


/**
 *	This method updates our local vars from the transform
 */
void EditorChunkVLO::updateLocalVars( const Matrix & m, Chunk * pChunk )
{
	if ( pObject_ && pChunk )
	{
		Matrix world(m);
		world.postMultiply( pChunk->transform() );
		pObject_->updateLocalVars(world);
	}
}


/**
 *	This method updates our caches of world space variables
 */
void EditorChunkVLO::updateWorldVars( const Matrix & m )
{
	if (pObject_)
		pObject_->updateWorldVars( m );
}

/**
 *	The overridden edShouldDraw method
 */
bool EditorChunkVLO::edShouldDraw()
{
	if( !EditorChunkSubstance<ChunkVLO>::edShouldDraw() )
		return false;
	// This is a hack for water, Since EditorChunkWater is not really a ChunkItem
	if( pOwnSect()->readString("type", "") == "water" )
		return Options::getOptionInt( "render/environment", 1 ) &&
			Options::getOptionInt( "render/environment/drawWater", 1 );
	return true;
}
/**
 *	
 */
void EditorChunkVLO::draw()
{
	if( !edShouldDraw() )
		return;

	if (pObject_)
	{	
		static uint32 s_settingsMark_ = -16;
		static int renderMiscShadeReadOnlyAreas = 1;
		if (Moo::rc().frameTimestamp() != s_settingsMark_)
		{
			renderMiscShadeReadOnlyAreas =
								Options::getOptionInt( "render/misc/shadeReadOnlyAreas", 1 );
			s_settingsMark_ = Moo::rc().frameTimestamp();
			pObject_->drawRed(false);
		}
		bool drawRed = !EditorChunkCache::instance( *pChunk_ ).edIsWriteable() && 
							renderMiscShadeReadOnlyAreas != 0;

		bool projectModule = ProjectModule::currentInstance() == ModuleManager::instance().currentModule();

		if (projectModule)
			pObject_->drawRed(false);
		else if( drawRed )
			pObject_->drawRed(true);
	}


	ChunkVLO::draw();
	if ( (drawTransient_ || BigBang::instance().drawSelection()) && pChunk_ && pObject_)
	{
		if( BigBang::instance().drawSelection() )
			Moo::rc().setRenderState( D3DRS_TEXTUREFACTOR, (DWORD)this );
		Moo::rc().push();
		Moo::rc().preMultiply( pObject_->localTransform(pChunk_) );
		static Matrix offset(Matrix::identity);
		offset.translation(Vector3(0.f,0.1f,0.f));
		Moo::rc().preMultiply( offset );
		Model::s_blendCookie_ = (Model::s_blendCookie_+1) & 0x0ffffff;

		ModelPtr mp = reprModel();
		mp->dress();	// should really be using a supermodel...
		mp->draw( true );

		Moo::rc().pop();
	}
}


/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkVLO::reprModel() const
{
	static ModelPtr waterModel =
		Model::get( "resources/models/water.model" );
	return waterModel;
}


/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkVLO, vlo, 1 )