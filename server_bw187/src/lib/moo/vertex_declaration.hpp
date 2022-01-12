/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VERTEX_DECLARATION_HPP
#define VERTEX_DECLARATION_HPP

#include "moo_dx.hpp"
#include <map>
#include "resmgr/datasection.hpp"
#include "cstdmf/concurrency.hpp"

namespace Moo
{

/**
 * This class handles the vertex declarations used by moo.
 * Vertex declarations are stored on disk, and created using this class.
 */

typedef ComObjectWrap< DX::VertexDeclaration > D3DVertexDeclarationPtr;

class VertexDeclaration
{
public:
	typedef std::vector<std::string > Aliases;
	VertexDeclaration();
	~VertexDeclaration();
	bool	load( DataSectionPtr pSection );
	const Aliases& aliases() const { return aliases_; }
	DX::VertexDeclaration* declaration() { return pDecl_.pComObject(); }
	
	static VertexDeclaration* get( const std::string& declName );
	static void fini();
private:
	D3DVertexDeclarationPtr	pDecl_;
	Aliases					aliases_;
	VertexDeclaration( const VertexDeclaration& );
	VertexDeclaration& operator=( const VertexDeclaration& );
	
	static SimpleMutex	declarationsLock_;

};

};


#endif // VERTEX_DECLARATION_HPP
