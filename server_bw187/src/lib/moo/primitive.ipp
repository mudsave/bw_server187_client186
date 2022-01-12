/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif

// INLINE void Primitive::inlineFunction()
// {
// }

namespace Moo
{

INLINE
uint32 Primitive::nPrimGroups( ) const
{
	return primGroups_.size();
}

INLINE
uint32 Primitive::maxVertices( ) const
{
	return maxVertices_;
}

INLINE
void Primitive::resourceID( const std::string& resourceID )
{
	resourceID_ = resourceID;
}

INLINE
const std::string& Primitive::resourceID( ) const
{
	return resourceID_;
}

INLINE
const Primitive::PrimGroup& Primitive::primitiveGroup( uint32 i ) const
{
#if !defined( _RELEASE )
	if ( ! (i < primGroups_.size() ) )
	{
		std::string buf( "primitiveGroup access violation, resource " );
		buf += resourceID_;
		MF_ASSERT( buf.c_str() );
		return primGroups_[ i % primGroups_.size() ];
	}	
#endif
	return primGroups_[ i ];
}

INLINE
D3DPRIMITIVETYPE Primitive::primType() const
{
	return primType_;
}

}

/*primitive.ipp*/
