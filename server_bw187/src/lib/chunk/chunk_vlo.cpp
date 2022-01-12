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
#include "chunk_vlo.hpp"

#include "moo/render_context.hpp"
#include "romp/geometrics.hpp"

#if UMBRA_ENABLE
#include <umbraObject.hpp>
#include <umbraModel.hpp>
#endif

#include "chunk.hpp"
#include "chunk_obstacle.hpp"
#include "chunk_space.hpp"
#include "chunk_manager.hpp"
#include "unique_id.hpp"

#include "resmgr/xml_section.hpp"

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )

VeryLargeObject::UniqueObjectList VeryLargeObject::s_uniqueObjects_;
#ifdef EDITOR_ENABLED
uint32 VeryLargeObject::currentSelectionMark_=0;
#endif

ChunkVLO::Factories * ChunkVLO::pFactories_ = NULL;


namespace
{
	SimpleMutex s_itemListMutex;
}


/**
 *	Constructor. Registers with chunk's static factory registry
 */
VLOFactory::VLOFactory(
		const std::string & section,
		int priority,
		Creator creator ) :
	priority_( priority ),
	creator_( creator )
{
	ChunkVLO::registerFactory( section, *this );
}


/**
 *	This virtual method calls the creator function that was passed in,
 *	as long as it's not NULL. It is called by a Chunk when it encounters
 *	the section name.
 *
 *	@return true if succeeded
 */
bool VLOFactory::create( Chunk * pChunk, DataSectionPtr pSection, 
						std::string uid ) const
{
	if (creator_ == NULL) return false;
	return (*creator_)( pChunk, pSection, uid );
}


/**
 *	This static method registers the input factory as belonging
 *	to the input section name. If there is already a factory
 *	registered by this name, then this factory supplants it if
 *	it has a (strictly) higher priority.
 */
void ChunkVLO::registerFactory( const std::string & section,
	const VLOFactory & factory )
{
	// avoid initialisation-order problems
	if (pFactories_ == NULL)
	{
		pFactories_ = new Factories();
	}

	// get a reference to the entry. if it's a new entry, the default
	// 'pointer' constructor will make it NULL.
	const VLOFactory *& pEntry = (*pFactories_)[ section ];

	// and whack it in
	if (pEntry == NULL || pEntry->priority() < factory.priority())
	{
		pEntry = &factory;
	}
}


// -----------------------------------------------------------------------------
// Section: ChunkVLO
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ChunkVLO::ChunkVLO( int wantFlags ) :
	ChunkItem( wantFlags ),
	pObject_( NULL ),
	dirty_( false ),
	creationRoot_( false )
{
}


/**
 *	Destructor.
 */
ChunkVLO::~ChunkVLO()
{
	if (pObject_)
	{
		pObject_->removeItem( this );
		pObject_ = NULL;
	}
}


/**
 *	Load a specific VLO
 */
bool ChunkVLO::loadItem( Chunk* pChunk, DataSectionPtr pSection )
{
	MF_ASSERT( pFactories_ != NULL );
	std::string type = pSection->readString( "type", "" );
	std::string uid = pSection->readString( "uid", "" );
	Factories::iterator found = pFactories_->find( type );	
	if (found != pFactories_->end() && (VeryLargeObject::getObject(uid) == NULL))
	{
        std::string file = pChunk->mapping()->path() + uid + ".vlo";
		DataSectionPtr pDS = BWResource::openSection( file + '/' + type );
		if (pDS && found->second->create( pChunk, pDS, uid ))
		{
#ifdef EDITOR_ENABLED
			VeryLargeObject::VeryLargeObjectPtr pObj = VeryLargeObject::getObject( uid );
			DataSectionPtr pVLOSection = BWResource::openSection( file );
			if (pVLOSection)
				pObj->section(pVLOSection);
#endif //EDITOR_ENABLED
			return true;
		}
		else
        {
            ERROR_MSG( "ChunkVLO::loadItem invalid or missing file %s", file.c_str());
			return false;
        }
	}
	return true;
}

//TODO: need an "editor" version of the VLO objects
//#ifndef EDITOR_ENABLED
//ChunkVLO* VeryLargeObject::createReference( std::string& uid, Chunk* pChunk )
//{
//	return NULL;
//}
//#endif // !EDITOR_ENABLED


#ifdef EDITOR_ENABLED


/**
 *	Construct the VLO's data section
 */
bool ChunkVLO::buildVLOSection( DataSectionPtr pObjectSection, Chunk* pChunk, std::string& type, std::string& uid )
{
	if (pObjectSection)
	{
		Factories::iterator found = pFactories_->find( type );
		if (found != pFactories_->end())
		{
			if (found->second->create( pChunk, pObjectSection, uid ))
			{
				pObject_ = VeryLargeObject::s_uniqueObjects_[uid];
				pObject_->addItem( this );
			}
		}
	}
	else
		return false;

	if ( pObject_ )
	{
		//setup the VLOs section..
		std::string fileName( uid + ".vlo" );

		DataSectionPtr ds = pChunk->mapping()->pDirSection();
		DataSectionPtr pVLOSection = ds->newSection( fileName );

		if (!pVLOSection)
			return false;

		DataSectionPtr waterSection = pVLOSection->newSection( type );
		waterSection->setParent( pVLOSection );
		pObject_->section( pVLOSection );
		pObject_->save();

		return true;
	}
	else
		return false;
}


/**
 *	ChunkVLO creation
 */
bool ChunkVLO::createVLO( DataSectionPtr pSection, Chunk* pChunk )
{
	std::string type = pSection->readString( "type", "" );
	std::string uid = pSection->readString( "uid", "" );
	if (uid == "" && type != "")
	{
		uid = VeryLargeObject::generateUID();
		return buildVLOSection( pSection->openSection( type ), pChunk, type, uid );
	}
	return false;
}

/**
 *	Creation path used for some legacy loading... (old water sections)
 */
bool ChunkVLO::createLegacyVLO( DataSectionPtr pSection, Chunk* pChunk, std::string& type )
{	
	if (type != "")
	{
		std::string uid = VeryLargeObject::generateUID();
		return buildVLOSection( pSection, pChunk, type, uid );
	}
	return false;
}


/**
 *	ChunkVLO creation
 */
bool ChunkVLO::cloneVLO( DataSectionPtr pSection, Chunk* pChunk, VeryLargeObject::VeryLargeObjectPtr pSource )
{
	std::string type = pSection->readString( "type", "" );
	if (type != "")
	{
		DataSectionPtr objectSection = pSource->section()->openSection(type);
		return buildVLOSection( objectSection, pChunk, type, VeryLargeObject::generateUID() );
	}
	return false;
}
#endif //EDITOR_ENABLED


/**
 *	ChunkVLO load method
 */
bool ChunkVLO::load( DataSectionPtr pSection, Chunk * pChunk )
{
	std::string uid = pSection->readString( "uid", "" );
	std::string type = pSection->readString( "type", "" );
	if (uid != "" && type != "")
	{
		bool ret = ChunkVLO::loadItem( pChunk, pSection );
		if (ret)
		{
			pObject_ = VeryLargeObject::getObject( uid );
			pObject_->addItem( this );
		}

		return ret;
	}
	return false;
}


void ChunkVLO::syncInit()
{
#if UMBRA_ENABLE
	// Get the bounding box of the vlo intersected with the chunk
	BoundingBox bb = pObject_->chunkBB( this->pChunk_ );
	if (!bb.insideOut())
	{
		// Create the umbra resources for the vlo
		pUmbraModel_ = UmbraModelProxy::getObbModel( bb.minBounds(), bb.maxBounds() );
		pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_ );
		Matrix m = Matrix::identity;
		Umbra::Cell* umbraCell = this->pChunk_->getUmbraCell();
		m = this->pChunk_->transform();
		pUmbraObject_->object()->setUserPointer( (void*)this );
		pUmbraObject_->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)m );
		pUmbraObject_->object()->setCell( umbraCell );
	}
#endif // UMBRA_ENABLE
	pObject_->syncInit(this);
}

/**
 *
 */
void ChunkVLO::lend( Chunk * pChunk )
{
	if (pObject_)
		pObject_->lend( pChunk);
}


/**
 *	Move this body of water around between chunks
 */
void ChunkVLO::toss( Chunk * pChunk )
{
	if (pChunk_)
	{
		removeCollisionScene( );
#if UMBRA_ENABLE
		if (pChunk == NULL && pObject_)
		{
			pObject_->unlend( pChunk_ );
		}
#endif //UMBRA_ENABLE
	}

	// call base class method
	this->ChunkItem::toss( pChunk );
}


/**
 *	Pass on the sway info
 */
void ChunkVLO::sway( const Vector3 & src, const Vector3 & dst, const float diameter )
{
	if (pObject_)
		pObject_->sway( src, dst, diameter );
}


/**
 *	Draw the referenced VLO
 */
void ChunkVLO::draw()
{
	//TODO: flag the object as drawn (here or inside the objects draw call?)
	if (pObject_ && pChunk_)
		pObject_->draw( pChunk_->space() );
}


/**
 *	Check if the VLO has a reference in the specified chunk already
 */
ChunkVLO* VeryLargeObject::containsChunk( Chunk * pChunk )
{
	SimpleMutexHolder holder(s_itemListMutex);

	ChunkItemList::iterator it;
	for (it = itemList_.begin(); it != itemList_.end(); it++)
	{
		if ((*it)->chunk() == pChunk)
			return (*it);
	}
	return NULL;
}
//
//
//bool ChunkVLO::legacyLoad( DataSectionPtr pSection, Chunk * pChunk, std::string& type )
//{
//	if (createLegacyVLO( pSection, pChunk, type ))
//	{
////		Matrix m( pObject_->edTransform() );
////		m.postMultiply( pChunk->transform() );
////		pObject_->updateLocalVars( m );
//
//		//edTransform(m,false);
//
////		type_ = type;
////		uid_ = pObject_->getUID();
//
////		vloTransform_.postMultiply( pObject_->origin() );
//
//		pObject_->addItem( this );
//		return true;
//	}
//	return false;
//}
//

/**
 *	This static method creates a VLO reference from the input section and adds
 *	it to the given chunk.
 */
ChunkItemFactory::Result ChunkVLO::create( Chunk * pChunk, DataSectionPtr pSection )
{
	//TODO: generalise the want flags...
    // wantsDraw, wantsSway, wantsNest.  The wantsNest option is specific to 
    // water and should be fixed when new VLOs are added.
	ChunkVLO * pVLO = new ChunkVLO( 1 | 4 | 8 );

	if (!pVLO->load( pSection, pChunk ))
	{
		delete pVLO;
		return ChunkItemFactory::Result( NULL,
			"Failed to create " + pSection->readString( "type", "<unknown type>" ) +
			" VLO " + pSection->readString( "uid", "<unknown id>" ) );
	}
	else
	{
		pChunk->addStaticItem( pVLO );
		return ChunkItemFactory::Result( pVLO );
	}
}


/// Static factory initialiser
ChunkItemFactory ChunkVLO::factory_( "vlo", 0, ChunkVLO::create );


// -----------------------------------------------------------------------------
// Section: VeryLargeObject
// -----------------------------------------------------------------------------

/**
 *	Constructor
 */
VeryLargeObject::VeryLargeObject()
	: rebuild_( false )
#ifdef EDITOR_ENABLED
	,deleted_( false ),
	dataSection_( NULL ),
	listModified_( false ),
	selectionMark_( 0 ),
	chunkPath_( "" )
#endif
{ 
}


/**
 *	Constructor
 */
VeryLargeObject::VeryLargeObject( std::string uid, std::string type ) :
	type_( type ),
	rebuild_( false )
#ifdef EDITOR_ENABLED
	,deleted_( false ),
	dataSection_( NULL ),
	listModified_( false ),
	selectionMark_( 0 ),
	chunkPath_( "" )
#endif
{
	setUID( uid );		 
}


/**
 *	Destructor
 */
VeryLargeObject::~VeryLargeObject()
{
}


#ifdef EDITOR_ENABLED

void VeryLargeObject::cleanup()
{
	UniqueObjectList::iterator it = s_uniqueObjects_.begin();
	for (;it != s_uniqueObjects_.end(); it++)
	{
		VeryLargeObjectPtr obj = (*it).second;
		if (obj)
			obj->saveFile(NULL);
	}
}

/**
 *	shall we draw?
 */
bool VeryLargeObject::edShouldDraw()
{
	if( !EditorChunkItem::hideAllOutside() )
		return true;

	SimpleMutexHolder holder(s_itemListMutex);
	for( ChunkItemList::iterator iter = itemList_.begin(); iter != itemList_.end(); ++iter )
	{
		if( !(*iter)->chunk()->isOutsideChunk() )
			return true;
	}
	return false;
}

/**
 *	Generate a unique id for a VLO
 */
std::string VeryLargeObject::generateUID()
{
	std::string s( UniqueID::generate() );
	for (uint i = 0; i < s.length(); i++)
		s[i] = tolower( s[i] );
	return s;
}


/**
 *	Mark all the references as dirty.
 */
void VeryLargeObject::dirtyReferences()
{
	SimpleMutexHolder holder(s_itemListMutex);
	//TODO: is this efficient? perhaps just generate a mark to invalidate the current marks in the refs?
	//use marks instead? easier imo... and wont re-dirty a gazillion refs
	ChunkItemList::iterator it = itemList_.begin();
	for (; it != itemList_.end(); it++)
	{
		(*it)->removeCollisionScene();
		(*it)->dirtyRef(true);
	}
}


/**
 *	Create / Dirty existing references according to the updated bounding box of the VLO
 */
void VeryLargeObject::updateReferences( Chunk* pChunk )
{
	if (pChunk != NULL)
	{
		BoundingBox& worldbb = this->boundingBox();
		ChunkVLO* focus = this->containsChunk( pChunk );
		if (focus == NULL)
			return;

		// un-dirtify the instigating reference
		focus->dirtyRef(false);
	
		int allInOwnChunk = pChunk->isOutsideChunk() ? 0 : -1;
		// assume it's not all within its own chunk if the item
		// is in an outside chunk (i.e. if bb test passes then that's
		// good enough to loan for us)

		// go through every bound portal
		Chunk::piterator pend = pChunk->pend();
		for (Chunk::piterator pit = pChunk->pbegin(); pit != pend; pit++)
		{
			if (!pit->hasChunk()) continue;

			Chunk * pConsider = pit->pChunk;

			// if it's not in that chunk's bounding box
			// then it definitely doesn't want it
			if (!worldbb.intersects( pConsider->boundingBox() )) continue;

			// if that's an outside chunk and the item is completely
			// within its own chunk then it also doesn't want it
			if (pConsider->isOutsideChunk())
			{
				// don't bother checking this for inside chunks since they're
				// not allowed to have interior chunks (i.e. bb is good enough)
				if (allInOwnChunk < 0)
				{
					// should really check if it's not completely within the union
					// of all interior chunks, but checking just its own is an OK
					// approximation ... if we had the hull tree at this stage then
					// we could do a different test using findChunkFromPoint, but we
					// don't, and it would miss some cases too, so this will do
					Vector3 bbpts[2] = { worldbb.minBounds(), worldbb.maxBounds() };
					Vector3 tpoint;	// this simple algorithm obviously only works
					int i;			// if our own chunk has no interior chunks
					for (i = 0; i < 8; i++)
					{
						tpoint.x = bbpts[(i>>0)&1].x;
						tpoint.y = bbpts[(i>>1)&1].y;
						tpoint.z = bbpts[(i>>2)&1].z;
						if (!pChunk->contains( tpoint )) break;
					}
					allInOwnChunk = (i == 8);
					// if we are all in our own chunk (and we are in an inside
					// chunk, which is the only way we get here), then we can't
					// be in this chunk too... and furthermore we can't be any
					// any other chunks at all, so we can just stop here
					if (allInOwnChunk) break;
				}
				// if it's all within its own chunk then it can't be in this one
				//if (allInOwnChunk) continue;
				// ... but since we only calculate allInOwnChunk if our chunk is
				// an inside chunk, and if it were true we would have stopped
				// the loop already, then there's no point checking it again here,
				// in fact allInOwnChunk can only be zero here.
				MF_ASSERT( !allInOwnChunk );
				// could make the code a bit neater but this way is more logical
			}
			//create a new reference in this chunk (if one doesnt already exist)
			ChunkVLO* reference = this->containsChunk( pConsider );
			if (reference == NULL)//no reference in this chunk yet...add it				
				reference = createReference( uid_, pConsider );
			else if (reference->dirtyRef())
				updateReferences( pConsider );

			reference->updateTransform( pConsider );
			reference->dirtyRef(false);			
		}
	}	
}

#endif //EDITOR_ENABLED

/**
 *	Assign a UID for this VLO, TODO: remove this function from prying public eyes...
 */
void VeryLargeObject::setUID( std::string uid ) 
{
	uid_ = uid;
	s_uniqueObjects_[uid_] = this;

}


/**
 *	Add a reference item
 */
void VeryLargeObject::addItem( ChunkVLO* item )
{
	SimpleMutexHolder holder(s_itemListMutex);
	itemList_.push_back( item );
	itemList_.sort( );
	itemList_.unique( );
#ifdef EDITOR_ENABLED
	listModified_ = true;
#endif
}


/**
 *	Remove a reference item
 */
void VeryLargeObject::removeItem( ChunkVLO* item)
{
	SimpleMutexHolder holder(s_itemListMutex);
	itemList_.remove( item );

	if (itemList_.size() == 0)
	{
		s_uniqueObjects_[uid_] = NULL;
	}
#ifdef EDITOR_ENABLED
	listModified_ = true;
#endif
}


// chunk_vlo.cpp