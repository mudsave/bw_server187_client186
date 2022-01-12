/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_VLO_HPP
#define EDITOR_CHUNK_VLO_HPP

#include "chunk/chunk_vlo.hpp"
#include "editor_chunk_substance.hpp"


/**
 *	This class is the editor version of a ChunkVLO
 */
class EditorChunkVLO : public EditorChunkSubstance<ChunkVLO>, public Aligned
{
	DECLARE_EDITOR_CHUNK_ITEM( EditorChunkVLO )
public:
	EditorChunkVLO( );	
	EditorChunkVLO( std::string type );	
	~EditorChunkVLO();

	bool load( std::string& uid, Chunk * pChunk );
	bool load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString = NULL );	
	
	std::string edDescription()
	{
		std::string iDesc;
		if (pObject_)
			iDesc = std::string(pObject_->edClassName()) + " Reference";
		else
			iDesc = "VLO Reference";

		return iDesc;
	}

	virtual bool edCanAddSelection() const 
	{ 
		return !dirtyRef() && chunk(); 
	}

	virtual bool edShouldDraw();
	virtual void draw();
	virtual void edPreDelete();
	virtual void edChunkSave();	
	virtual void edPostCreate();
	virtual void toss( Chunk * pChunk );
	virtual void removeCollisionScene();
	virtual const Matrix & edTransform();	
	virtual bool edSave( DataSectionPtr pSection );	
	virtual void edCloneSection( Chunk* destChunk, const Matrix& destMatrixInChunk, DataSectionPtr destDS );
	virtual bool edPreChunkClone( Chunk* srcChunk, const Matrix& destChunkMatrix, DataSectionPtr chunkDS );
	virtual bool edIsPositionRelativeToChunk() {	return false;	}
	virtual bool edBelongToChunk();
	virtual void updateTransform( Chunk * pChunk );
	virtual bool edEdit( class ChunkItemEditor & editor );
	virtual bool edTransform( const Matrix & m, bool transient );
	virtual bool legacyLoad( DataSectionPtr pSection, Chunk * pChunk, std::string& type );

	virtual bool edCheckMark( uint32 mark ) 
	{ 
		if (pObject_)
			return pObject_->edCheckMark(mark);
		return false;
	}


	void touch();
protected:
	virtual void objectCreated();
private:
	EditorChunkVLO( const EditorChunkVLO& );
	EditorChunkVLO& operator=( const EditorChunkVLO& );

	void updateWorldVars( const Matrix & m );
	void updateLocalVars( const Matrix & m, Chunk * pChunk );	

	virtual void addAsObstacle() { }	
	virtual ModelPtr reprModel() const;
	virtual const char * sectName() const { return "vlo"; }
	virtual const char * drawFlag() const { return "render/drawVLO"; }
	virtual bool edAffectShadow() const {	return pOwnSect()->readString("type") != "water"; }

	std::string		uid_;
	std::string		type_;
	Vector3			localPos_;
	bool			colliding_;
	Matrix			transform_;
	Matrix			vloTransform_;	
	bool			drawTransient_;
};

typedef SmartPointer<EditorChunkVLO> EditorChunkVLOPtr;

#endif // EDITOR_CHUNK_VLO_HPP
