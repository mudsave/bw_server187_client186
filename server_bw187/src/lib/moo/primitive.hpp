/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PRIMITIVE_HPP
#define PRIMITIVE_HPP

#include <iostream>
#include "moo_math.hpp"
#include "moo_dx.hpp"
#include "com_object_wrap.hpp"
#include "cstdmf/smartpointer.hpp"
#include "material.hpp"
#include "index_buffer.hpp"

namespace Moo
{

typedef SmartPointer< class Primitive > PrimitivePtr;

class Primitive : public SafeReferenceCount
{
public:
	typedef struct
	{
		int		startIndex_;
		int		nPrimitives_;
		int		startVertex_;
		int		nVertices_;
	}PrimGroup;

	virtual HRESULT		setPrimitives();
	virtual HRESULT		drawPrimitiveGroup( uint32 groupIndex );
	virtual HRESULT		release( );
	virtual HRESULT		load( );

	uint32				nPrimGroups() const;
	const PrimGroup&	primitiveGroup( uint32 i ) const;
	uint32				maxVertices() const;

	const std::string&	resourceID() const;
	void				resourceID( const std::string& resourceID );

	D3DPRIMITIVETYPE	primType() const;

	bool				verifyIndices( uint32 maxVertex );

	const IndicesHolder& indices() const { return indices_; }

	Primitive( const std::string& resourceID );
	virtual ~Primitive();

protected:
	
	typedef std::vector< PrimGroup > PrimGroupVector;

	
	PrimGroupVector		primGroups_;
	
	uint32				nIndices_;
	uint32				maxVertices_;
	std::string			resourceID_;
	D3DPRIMITIVETYPE	primType_;
	IndicesHolder		indices_;

	IndexBuffer			indexBuffer_;

private:


/*	Primitive(const Primitive&);
	Primitive& operator=(const Primitive&);*/

	friend std::ostream& operator<<(std::ostream&, const Primitive&);
};

}

#ifdef CODE_INLINE
#include "primitive.ipp"
#endif




#endif