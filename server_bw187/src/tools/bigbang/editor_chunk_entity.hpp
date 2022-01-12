/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_ENTITY_HPP
#define EDITOR_CHUNK_ENTITY_HPP

#include "chunk/chunk_item.hpp"
#include "editor_chunk_substance.hpp"
#include "cstdmf/concurrency.hpp"
#include "entitydef/entity_description_map.hpp"
#include "romp/super_model.hpp"
#include "gizmo/general_editor.hpp"

class EntityDescription;
#include "pyscript/pyobject_plus.hpp"

class EditorChunkStationNode;
class MatrixProxy;
class EditorChunkLink;
typedef SmartPointer<EditorChunkLink> EditorChunkLinkPtr;


/**
 *	This class is the editor version of an entity in a chunk.
 *
 *	Note that it does not derive from the client's version of a chunk entity
 *	(because the editor does not have the machinery necessary to implement it)
 */
class EditorChunkEntity : public EditorChunkSubstance<ChunkItem>, public Aligned
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkEntity )
public:
	EditorChunkEntity();
	~EditorChunkEntity();

	bool load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString = NULL );
	virtual void edMainThreadLoad();
	bool edLoad( const std::string type, DataSectionPtr pSection, std::string* errorString = NULL );
	bool edLoad( DataSectionPtr pSection );

	virtual bool edSave( DataSectionPtr pSection );

    virtual void toss( Chunk * pChunk );

	virtual const Matrix & edTransform()	{ return transform_; }
	virtual bool edTransform( const Matrix & m, bool transient );

	virtual void draw();

    virtual void tick(float dtime);

	virtual std::string edDescription();
	virtual bool edAffectShadow() const {	return false; }

	virtual bool edEdit( class ChunkItemEditor & editor );
	GeneralProperty* parseWidgetInt(
		int i, PyObject* pyEnum, DataDescription* pDD, MatrixProxy * pMP );
	GeneralProperty* parseWidgetFloat(
		int i, PyObject* pyEnum, DataDescription* pDD, MatrixProxy * pMP );
	bool edEditProperties( class ChunkItemEditor & editor, MatrixProxy * pMP );

	std::string typeGet() const;
	bool typeSet( const std::string & s )	{ return false; }

	bool clientOnlyGet() const			{ return clientOnly_; }
	bool clientOnlySet( const bool & b );

	std::string lseGet() const				{ return lastScriptError_; }
	bool lseSet( const std::string & s )	{ return false; }

	std::string propGetString( int index ) const;
	bool propSetString( int index, const std::string & s );
	float propGetFloat( int index );
	bool propSetFloat( int index, float f );
	DataSectionPtr propGet( int index );
	bool propSet( int index, DataSectionPtr pTemp );
	PyObject * propGetPy( int index );
	bool propSetPy( int index, PyObject * pObj );
    int propCount() const;

	int propGetInt( int index );
	bool propSetInt( int index, int f );

	std::string propName( int index );

	Vector3 edMovementDeltaSnaps();
	float edAngleSnaps();

	/** Set the correct model for all entities that have changed recently */
	static void calculateDirtyModels();

	void setSurrogateMarker( DataSectionPtr pMarkerSect )
		{ surrogateMarker_ = true; pOwnSect_ = pMarkerSect; }
	void clearProperties();
	
	ModelPtr getReprModel() const { return reprModel(); }
	bool isDefaultModel() const;
	void clearEditProps();
	void setEditProps( const std::list< std::string > & names );
	void clearPropertySection();
	
	const EntityDescription * getTypeDesc() { return pType_; }

	typedef std::pair< PyObject *, std::string > BindingProperty;
	typedef std::vector< BindingProperty > BindingProperties;
	const BindingProperties & getBindingProps() { return bindingProps_; }

	void resetSelUpdate();	// i.e. reload the data section

    virtual bool isEditorEntity() const { return true; }

    void patrolListNode(std::string const &id);

    std::string patrolListNode() const;

    std::string patrolListGraphId() const;

    int patrolListPropIdx() const;

	bool patrolListRelink( const std::string& newGraph, const std::string& newNode );

    void disconnectFromPatrolList();

private:
	EditorChunkEntity( const EditorChunkEntity& );
	EditorChunkEntity& operator=( const EditorChunkEntity& );

	virtual const char * sectName() const { return "entity"; }
	virtual const char * drawFlag() const { return "render/drawEntities"; }
	virtual ModelPtr reprModel() const;

	void recordBindingProps();
	EditorChunkStationNode* connectedNode();

	std::string nodeId() const                   { return patrolListNode(); }
	bool nodeId( const std::string & /*s*/ )     { return false; }

	std::string graphId() const                  { return patrolListGraphId(); }
	bool graphId( const std::string & /*s*/ )    { return false; }

	bool initType( const std::string type, std::string* errorString );

	DataSectionPtr pOriginalSect_;
	const EntityDescription *	pType_;
	Matrix			transform_;
	bool			clientOnly_;

	std::string		lastScriptError_;

	PyObject *			pDict_;
	std::vector<bool>	usingDefault_;
	std::vector<bool>	allowEdit_;		// only used if surrogateMarker_

	ModelPtr			model_;

	PyObject *			pyClass_;

	bool				surrogateMarker_;

    std::string         patrolListNode_;

    EditorChunkLinkPtr  link_;

	/** Which entities need their model recalculated */
	static std::vector<EditorChunkEntity*> s_dirtyModelEntities_;

	BindingProperties bindingProps_;

	void markModelDirty();
	void removeFromDirtyList();

	void calculateModel();

	static SimpleMutex s_dirtyModelMutex_;
};

typedef SmartPointer<EditorChunkEntity> EditorChunkEntityPtr;



// -----------------------------------------------------------------------------
// Section: EditorEntityType
// -----------------------------------------------------------------------------


class EditorEntityType
{
public:
	static void startup();
	static void shutdown();

	EditorEntityType();

	const EntityDescription * get( const std::string & name );
	PyObject * getPyClass( const std::string & name );

	static EditorEntityType & instance();
	bool isEntity( const std::string& name ) const	{	return eMap_.isEntity( name );	}
private:
	EntityDescriptionMap	eMap_;
	std::map<std::string,EntityDescription*> mMap_;

	StringHashMap< PyObject * > pyClasses_;

	static EditorEntityType * s_instance_;
};


#endif // EDITOR_CHUNK_ENTITY_HPP
