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

// INLINE void DynamicVertexBufferBase::inlineFunction()
// {
// }

namespace Moo
{
INLINE
void DynamicVertexBufferBase::release( )
{
	lockBase_ = 0;
	vertexBuffer_.pComObject( NULL );
}

INLINE

void DynamicVertexBufferBase::resetLock( )
{
	reset_ = true;	
}

INLINE
HRESULT DynamicVertexBufferBase::unlock( )
{
//	MF_ASSERT( locked_ == true );
	locked_ = false;
	return vertexBuffer_->Unlock( );
}

INLINE
DX::VertexBuffer* DynamicVertexBufferBase::pVertexBuffer( )
{
//	MF_ASSERT( vertexBuffer_.pComObject() != NULL );
	return vertexBuffer_.pComObject();
}



}
/*dynamic_vertex_buffer.ipp*/
