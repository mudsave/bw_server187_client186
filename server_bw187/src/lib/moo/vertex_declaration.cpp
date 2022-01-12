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
#include "vertex_declaration.hpp"
#include "resmgr/bwresource.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 );


// -----------------------------------------------------------------------------
// Section: VertexDeclaration
// -----------------------------------------------------------------------------

namespace Moo
{

SimpleMutex	VertexDeclaration::declarationsLock_;


struct DeclType
{
	DeclType( D3DDECLTYPE t, uint32 s )
	:	declType_( t),
		size_( s )
	{}
	D3DDECLTYPE declType_;
	uint32 size_;
};
typedef std::map< std::string, DeclType > DeclTypeMap;
#define DTM_ENTRY( x, y ) dtm.insert( std::make_pair( #x, DeclType( D3DDECLTYPE_##x, y ) ) );

static DeclType type( const std::string& type )
{
	static DeclTypeMap dtm;
	if (dtm.empty())
	{
		DTM_ENTRY( FLOAT1, 4 );
		DTM_ENTRY( FLOAT2, 8 );
		DTM_ENTRY( FLOAT3, 12 );
		DTM_ENTRY( FLOAT4, 16 );
		DTM_ENTRY( D3DCOLOR, 4 );
		DTM_ENTRY( UBYTE4, 4 );
		DTM_ENTRY( SHORT2, 4 );
		DTM_ENTRY( SHORT4, 8 );
		DTM_ENTRY( UBYTE4N, 4 );
		DTM_ENTRY( SHORT2N, 4 );
		DTM_ENTRY( SHORT4N, 8 );
		DTM_ENTRY( USHORT2N, 4 );
		DTM_ENTRY( USHORT4N, 8 );
		DTM_ENTRY( UDEC3, 4 );
		DTM_ENTRY( DEC3N, 4 );
		DTM_ENTRY( FLOAT16_2, 4 );
		DTM_ENTRY( FLOAT16_4, 8 );
	}

	DeclTypeMap::iterator it = dtm.find( type );
	if (it != dtm.end())
		return it->second;

	ERROR_MSG( "VertexDeclaration::type - unable to find declaration type %s\n", type.c_str() );
	return DeclType( D3DDECLTYPE_UNUSED, 0 );
}

typedef std::map< std::string, D3DDECLUSAGE > DeclUsageMap;
#define DUM_ENTRY( x ) dum.insert( std::make_pair( #x, D3DDECLUSAGE_##x ) );

static D3DDECLUSAGE usage( const std::string& usage )
{
	static DeclUsageMap dum;
	if (dum.empty())
	{
		DUM_ENTRY( POSITION );
		DUM_ENTRY( BLENDWEIGHT );
		DUM_ENTRY( BLENDINDICES );
		DUM_ENTRY( NORMAL );
		DUM_ENTRY( PSIZE );
		DUM_ENTRY( TEXCOORD );
		DUM_ENTRY( TANGENT );
		DUM_ENTRY( BINORMAL );
		DUM_ENTRY( TESSFACTOR );
		DUM_ENTRY( POSITIONT );
		DUM_ENTRY( COLOR );
		DUM_ENTRY( FOG );
		DUM_ENTRY( DEPTH );
		DUM_ENTRY( SAMPLE );
	}

	DeclUsageMap::iterator it = dum.find( usage );
	if (it != dum.end())
		return it->second;
	ERROR_MSG( "VertexDeclaration::usage - unable to find declaration usage %s\n", usage.c_str() );
	return (D3DDECLUSAGE)(MAXD3DDECLUSAGE + 1);
}
/**
 *	Constructor.
 */
VertexDeclaration::VertexDeclaration()
{
}

/**
 *	Destructor.
 */
VertexDeclaration::~VertexDeclaration()
{
}

bool VertexDeclaration::load( DataSectionPtr pSection )
{
	bool ret = false;
	std::vector< D3DVERTEXELEMENT9 >	elements;
	D3DVERTEXELEMENT9 currentElement;
	ZeroMemory( &currentElement, sizeof( currentElement ) );

	DataSectionIterator it = pSection->begin();
	DataSectionIterator end = pSection->end();
	while( it != end )
	{
		DataSectionPtr pDS = *it;
		const std::string& key = pDS->sectionName();
		if (key == "alias")
		{
			aliases_.push_back(pDS->asString());
		}
		else
		{
			D3DDECLUSAGE u = usage( key );
			if (u <= MAXD3DDECLUSAGE)
			{
				currentElement.Usage = u;
				currentElement.UsageIndex = (*it)->asInt();
				currentElement.Stream = pDS->readInt( "stream", currentElement.Stream );
				currentElement.Offset = pDS->readInt( "offset", currentElement.Offset );
				DeclType dt = type( pDS->readString( "type", "FLOAT3" ) );
				currentElement.Type = dt.declType_;
				elements.push_back( currentElement );
				currentElement.Offset += WORD(dt.size_);
				ret = true;
			}
		}
		it++;
	}

	if (ret)
	{
		const D3DVERTEXELEMENT9 theend = D3DDECL_END();
		DX::Device* pDevice = rc().device();
		elements.push_back( theend );
		IDirect3DVertexDeclaration9* pDecl = NULL;
		if (SUCCEEDED(pDevice->CreateVertexDeclaration( &elements.front(), &pDecl )))
		{
			pDecl_ = pDecl;
			pDecl->Release();
			pDecl = NULL;
		}
		else
		{
			ERROR_MSG( "VertexDeclaration::load - Unable to create vertex declaration\n" );
			ret = false;
		}

	}
	return ret;
}

typedef std::map< std::string, VertexDeclaration* > DeclMap;
static DeclMap s_declMap;

VertexDeclaration* VertexDeclaration::get( const std::string& declName )
{
	SimpleMutexHolder smh( declarationsLock_ );

	DeclMap::iterator it = s_declMap.find( declName );

	if (it == s_declMap.end())
	{
		DataSectionPtr pSection = BWResource::instance().openSection( std::string("shaders/formats/") + declName + ".xml" );
		VertexDeclaration* vd = new VertexDeclaration();
		if (pSection && vd->load( pSection ))
		{
			s_declMap[declName] = vd;
			return vd;
		}
		delete vd;
		return NULL;
	}
	return it->second;
}


void VertexDeclaration::fini()
{
	DeclMap::iterator it = s_declMap.begin();
	DeclMap::iterator end = s_declMap.end();
	while (it != end)
	{
		delete it->second;
		++it;
	}
}


};

// vertex_declaration.cpp
