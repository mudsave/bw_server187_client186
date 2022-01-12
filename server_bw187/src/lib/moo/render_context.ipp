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

extern uint32 g_setRSCallCount;
extern uint32 g_setRSChangeCount;
extern uint32 g_setTSSCallCount;
extern uint32 g_setTSSChangeCount;
extern uint32 g_setTextureCallCount;
extern uint32 g_setTextureChangeCount;

// INLINE void RenderContext::inlineFunction()
// {
// }

namespace Moo
{

/**
 *	This method returns the d3d device created by the render context.
 */
INLINE
DX::Device* RenderContext::device() const
{
	return device_.pComObject();
}

/**
 * This method returns whether the display is running in windowed mode or not.
 */
INLINE
bool RenderContext::windowed( ) const
{
	return windowed_;
}


/**
 * This method returns whether a stencil buffer is available
 */
INLINE
bool RenderContext::stencilAvailable( ) const
{
	return stencilAvailable_;
}

/**
 * This method returns whether a 32 bits index buffer is available
 */
INLINE
DWORD RenderContext::maxVertexIndex( ) const
{
	return devices_[ 0 ].caps_.MaxVertexIndex;
}

/**
 *	This method returns the index of the current device,
 */
INLINE
uint32 RenderContext::deviceIndex( ) const
{
	return deviceIndex_;
}


/**
 *	This method returns the index of the current displaymode.
 */
INLINE
uint32 RenderContext::modeIndex( ) const
{
	return modeIndex_;
}

/**
 *	This method returns a reference to the current camera.
 */
INLINE
const Camera& RenderContext::camera( ) const
{
	return camera_;
}

/**
 *	@see camera
 */
INLINE
Camera& RenderContext::camera( )
{
	return camera_;
}

/**
 *	@see camera
 */
INLINE
void RenderContext::camera( const Camera& cam )
{
	camera_ = cam;
}


/**
 *	This method returns the current projection matrix.
 */
INLINE
const Matrix& RenderContext::projection( ) const
{
	return projection_;
}

/**
 *	@see projection
 */
INLINE
Matrix& RenderContext::projection( )
{
	return projection_;
}

/**
 *	@see projection
 */
INLINE
void RenderContext::projection( Matrix& m )
{
	projection_ = m;
	viewProjection_.multiply( view_, projection_ );
}

/**
 *	This method returns the current view matrix.
 */
INLINE
const Matrix& RenderContext::view( ) const
{
	return view_;
}

/**
 *	@see view
 */
INLINE
Matrix& RenderContext::view( )
{
	return view_;
}

/**
 *	@see view
 */
INLINE
void RenderContext::view( const Matrix& m )
{
	view_ = m;
	viewProjection_.multiply( view_, projection_ );
}

/**
 *	This method returns the current world matrix.
 */
INLINE
const Matrix& RenderContext::world() const
{
	return world_.back();
}

/**
 *	@see world.
 */
INLINE
Matrix& RenderContext::world()
{
	return world_.back();
}

/**
 *	@see world.
 */
INLINE
void RenderContext::world( const Matrix& m )
{
	world_.back() = m;
}

/**
 *	This method multiplies the input by the world matrix ( m * world )
 */
INLINE
void RenderContext::preMultiply( const Matrix& m )
{
	world_.back().preMultiply( m );
}

/**
 *	This method multiplies the world matrix by the input ( world * m )
 */
INLINE
void RenderContext::postMultiply( const Matrix& m )
{
	world_.back().postMultiply( m );
}


/**
 *	Push the world matrix.
 */
INLINE
void RenderContext::push( )
{
	MF_ASSERT( world_.size() > 0 );
	world_.push_back( world_.back() );
}

/**
 *	Pop the world matrix.
 */
INLINE
void RenderContext::pop( )
{
	MF_ASSERT( world_.size() > 1 );
	world_.pop_back();
}

/**
 *	This method returns the view * projection matrix.
 */
INLINE
const Matrix& RenderContext::viewProjection( ) const
{
	return viewProjection_;
}

/**
 *	This method returns the inverse view matrix.
 */
INLINE
const Matrix& RenderContext::invView( ) const
{
	return invView_;
}

/**
 *	This method returns the number of enumerated devices.
 */
INLINE
uint32 RenderContext::nDevices( ) const
{
	return devices_.size();
}

/**
 *	This method returns the device info of a device.
 *	@param i the index of the device.
 */
INLINE
const Moo::DeviceInfo& RenderContext::deviceInfo( uint32 i ) const
{
	MF_ASSERT( devices_.size() > i );
	return devices_[ i ];
}

/**
 *	This method resets the world matrix to identity and the world matrix stack.
 */
INLINE
void RenderContext::reset( )
{
	world_.clear();
	Matrix m;
	m.setIdentity();
	world_.push_back( m );
}

/**
 *	This method resets the world matrix to the supplied matrix and the world matrix stack.
 *	@param m the matrix to reset the world matrix to.
 */
INLINE
void RenderContext::reset( const Matrix& m )
{
	world_.clear();
	world_.push_back( m );
}

/**
 *	This method returns the description of the back buffer.
 */
INLINE
const D3DSURFACE_DESC& RenderContext::backBufferDesc( ) const
{
	return backBufferDesc_;
}

/**
 *	This method returns the current presentation paramaters.
 */
INLINE
const D3DPRESENT_PARAMETERS& RenderContext::presentParameters( ) const
{
	return presentParameters_;
}

/**
 *	This method adds n to the internal primitive count.
 */
INLINE
void RenderContext::addToPrimitiveCount( uint32 n )
{
	primitiveCount_ += n;
}

/**
 *	This method returns the internal primitivecount.
 */
INLINE
uint32 RenderContext::primitiveCount( ) const
{
	return primitiveCount_;
}

/**
 *	This method resets the primitvecount.
 *	@return the number of primitves in the count before reset.
 */
INLINE
uint32 RenderContext::resetPrimitiveCount( )
{
	uint32 pc = primitiveCount_;
	primitiveCount_ = 0;
	return pc;
}

/**
 *	This method returns the current frametimestamp,
 *	the frametimestamp is increased every time nextframe is called.
 */
INLINE
uint32 RenderContext::frameTimestamp( ) const
{
	return currentFrame_;
}

/**
 *	This method let's you query whether a frame has been drawn or not.
 *	@param frame gets updated if it's not equal to the current frametimestamp.
 *	@return true if frame is equal to the frame timestamp.
 */
INLINE
bool RenderContext::frameDrawn( uint32& frame ) const
{
	bool res = frame == currentFrame_;
	frame = currentFrame_;

	return res;
}

/**
 *	This method adds n to the internal primitive group count.
 */
INLINE
void RenderContext::addToPrimitiveGroupCount( uint32 n )
{
	primitiveGroupCount_ += n;
}

/**
 *	This method returns the internal primitive  group count.
 */
INLINE
uint32 RenderContext::primitiveGroupCount( ) const
{
	return primitiveGroupCount_;
}

/**
 *	This method resets the primitve  group count.
 *	@return the number of primitve groups  in the count before reset.
 */
INLINE
uint32 RenderContext::resetPrimitiveGroupCount( )
{
	uint32 val = primitiveGroupCount_;
	primitiveGroupCount_ = 0;
	return val;
}

/**
 *	This method returns the distance from the near plane to the beginning of the fog.
 */
INLINE
float RenderContext::fogNear( ) const
{
	return fogNear_;
}

/**
 *	@see fogNear
 */
INLINE
void RenderContext::fogNear( float fogNear )
{
	fogNear_ = fogNear;
}

/**
 *	This method returns the distance from the near plane to the end of the fog.
 */
INLINE
float RenderContext::fogFar( ) const
{
	return fogFar_;
}

/**
 * @see fogFar
 */
INLINE
void RenderContext::fogFar( float fogFar )
{
	fogFar_ = fogFar;
}

/**
 *	This method returns the current light container.
 */
INLINE
LightContainerPtr RenderContext::lightContainer( ) const
{
	return lightContainer_;
}

/**
 *	@see lightContainer.
 */
INLINE
void RenderContext::lightContainer( LightContainerPtr pLC )
{
	lightContainer_ = pLC;
}

/**
 *	This method returns the current light container of specular lights.
 */
INLINE
LightContainerPtr RenderContext::specularLightContainer( ) const
{
	return specularLightContainer_;
}

/**
 *	@see specularLightContainer.
 */
INLINE
void RenderContext::specularLightContainer( LightContainerPtr pLC )
{
	specularLightContainer_ = pLC;
}

/**
 *	This method returns the object alpha override, if this value is set,
 *	the objects that get rendered should be rendered with an opacity.
 */
INLINE
uint32 RenderContext::objectAlphaOverride() const
{
	return alphaOverride_;
}

/**
 *	@see objectAlphaOverride
 */
INLINE
void RenderContext::objectAlphaOverride( float alpha )
{
	if ( alpha < 0.999f )
	{
		alphaOverride_ = ( (uint32)( alpha * 255.f ) ) << 24;
	}
	else
	{
		alphaOverride_ = 0xff000000;
	}
}

/**
 *	TODO
 */
INLINE
float RenderContext::lodValue() const
{
	return lodValue_;
}

/**
 *	@see lodValue.
 */
INLINE
void RenderContext::lodValue( float value)
{
	lodValue_ = max( 0.f, min( 1.f, value ) );
}

/**
 * TODO
 */
INLINE
float RenderContext::lodFar() const
{
	return lodFar_;
}

/**
 * TODO
 */
INLINE
void RenderContext::lodFar( float value )
{
	lodFar_ = max( value, 0.f );
}

/**
 * TODO
 */
INLINE
float RenderContext::lodPower() const
{
	return lodPower_;
}

/**
 * TODO
 */
INLINE
void RenderContext::lodPower( float value )
{
	lodPower_ = max( value, 0.f );
}

/**
 *	TODO
 */
INLINE
float RenderContext::zoomFactor() const
{
	return zoomFactor_;
}

/**
 * TODO
 */
INLINE
int RenderContext::RenderTargetStack::nStackItems()
{
	return stackItems_.size();
}

/**
 * This method pushes the current renderTarget.
 */
INLINE
bool RenderContext::pushRenderTarget()
{
	return renderTargetStack_.push( this );
}

/**
 * This method pops the current renderTarget.
 */
INLINE
bool RenderContext::popRenderTarget()
{
	return renderTargetStack_.pop( this );
}


/**
 *	This methode returns the screen width in pixels.
 */
INLINE
float RenderContext::screenWidth() const
{
	return halfScreenWidth_ + halfScreenWidth_;
}

/**
 *	This methode returns the screen height in pixels.
 */
INLINE
float RenderContext::screenHeight() const
{
	return halfScreenHeight_ + halfScreenHeight_;
}


/**
 * This method returns the half screen width in pixels.
 */
INLINE
float RenderContext::halfScreenWidth() const
{
	return halfScreenWidth_;
}

/**
 *	This method returns the half screen height in pixels.
 */
INLINE
float RenderContext::halfScreenHeight() const
{
	return halfScreenHeight_;
}

/**
 *	This method returns the full screen aspect ratio.
 */
INLINE
float RenderContext::fullScreenAspectRatio() const
{
	return fullScreenAspectRatio_;
}

/**
 *	This method sets the full screen aspect ratio.
 */
INLINE
void RenderContext::fullScreenAspectRatio( float ratio )
{
	fullScreenAspectRatio_ = ratio;
}

/**
 *	This method sets the gamma correction value.
 */
INLINE
void RenderContext::gammaCorrection( float gammaCorrection )
{
	gammaCorrection_ = gammaCorrection;
	setGammaCorrection();
}

/**
 *	This method gets the gamma correction value.
 */
INLINE
float RenderContext::gammaCorrection( ) const
{
	return gammaCorrection_;
}


/**
 *	TODO
 */
INLINE
bool RenderContext::waitForVBL() const
{
	return waitForVBL_;
}

/**
 *	TODO
 */
INLINE
void RenderContext::waitForVBL( bool wait )
{
	waitForVBL_ = wait;
}

INLINE uint16 RenderContext::vsVersion() const
{
	return vsVersion_;
}

INLINE uint16 RenderContext::psVersion() const
{
	return psVersion_;
}

INLINE uint16 RenderContext::maxSimTextures() const
{
	return maxSimTextures_;
}

INLINE uint32 RenderContext::setRenderState(D3DRENDERSTATETYPE state, uint32 value)
{
	MF_ASSERT_DEBUG( state < ARRAY_SIZE(rsCache_) );

	if (value != rsCache_[state].currentValue)
	{
		rsCache_[state].currentValue = value;
		return device_->SetRenderState( state, value );
	}
	return D3D_OK;
}

INLINE uint32  RenderContext::getRenderState(D3DRENDERSTATETYPE state )
{
	MF_ASSERT_DEBUG( state < ARRAY_SIZE(rsCache_) );

	return rsCache_[state].currentValue;
}



INLINE uint32 RenderContext::setTextureStageState( uint32 stage, D3DTEXTURESTAGESTATETYPE type, uint32 value)
{
	MF_ASSERT_DEBUG( stage < ARRAY_SIZE(tssCache_) );
	MF_ASSERT_DEBUG( type < ARRAY_SIZE(tssCache_[stage]) );

    if( value != tssCache_[stage][type].currentValue)
    {
		tssCache_[stage][type].currentValue = value;
		return device_->SetTextureStageState( stage, type, value );
	}
    return D3D_OK;
}

INLINE uint32 RenderContext::setSamplerState( uint32 stage, D3DSAMPLERSTATETYPE type, uint32 value)
{
	MF_ASSERT_DEBUG( stage < ARRAY_SIZE(sampCache_) );
	MF_ASSERT_DEBUG( type < ARRAY_SIZE(sampCache_[stage]) );

    if( value != sampCache_[stage][type].currentValue)
    {
		sampCache_[stage][type].currentValue = value;
		return device_->SetSamplerState( stage, type, value );
	}
    return D3D_OK;
}

INLINE uint32 RenderContext::setVertexShader( DX::VertexShader* pVS )
{
	return device_->SetVertexShader( pVS );
}

INLINE uint32 RenderContext::setPixelShader( DX::PixelShader* pPS )
{
	return device_->SetPixelShader( pPS );
}

INLINE HRESULT RenderContext::setIndices( ComObjectWrap<DX::IndexBuffer> pIB )
{
	return device_->SetIndices( pIB.pComObject() );
}

INLINE ComObjectWrap<DX::IndexBuffer> RenderContext::getIndices() const
{
	ComObjectWrap<DX::IndexBuffer> pIB;
	device_->GetIndices( &pIB );
	return pIB;
}

INLINE uint32 RenderContext::setVertexDeclaration( DX::VertexDeclaration* pVD )
{
	if (pVD != vertexDeclaration_)
	{
		vertexDeclaration_ = pVD;
		fvf_ = 0;
		return device_->SetVertexDeclaration( pVD );
	}
    return D3D_OK;
}

INLINE uint32 RenderContext::setFVF( uint32 fvf )
{
	if (fvf != fvf_)
	{
		fvf_ = fvf;
		vertexDeclaration_ = NULL;
		return device_->SetFVF( fvf_ );
	}

	return D3D_OK;
}

INLINE uint32 RenderContext::setTexture(DWORD stage, DX::BaseTexture* pTexture)
{
	MF_ASSERT_DEBUG( stage < ARRAY_SIZE(textureCache_) );

    if(pTexture != textureCache_[stage].pCurrentTexture)
    {
        textureCache_[stage].pCurrentTexture = pTexture;
		return device_->SetTexture( stage, pTexture );
	}
	return D3D_OK;
}

INLINE uint32 RenderContext::drawIndexedPrimitiveUP( D3DPRIMITIVETYPE primitiveType,
													 UINT minIndex,
													 UINT numVertices,
													 UINT primitiveCount,
													 CONST void* pIndexData,
													 D3DFORMAT indexDataFormat,
													 CONST void* pVertexStreamZeroData,
													 UINT vertexStreamZeroStride )
{
	return device_->DrawIndexedPrimitiveUP( primitiveType, minIndex,numVertices,
											primitiveCount, pIndexData, indexDataFormat,
											pVertexStreamZeroData, vertexStreamZeroStride );
}


INLINE uint32 RenderContext::drawIndexedPrimitive( D3DPRIMITIVETYPE type,
														  INT  baseVertexIndex,
														  UINT minIndex,
														  UINT numVertices,
														  UINT startIndex,
														  UINT primitiveCount )
{
	return device_->DrawIndexedPrimitive( type, baseVertexIndex, minIndex, numVertices,
										  startIndex, primitiveCount );
}

INLINE uint32 RenderContext::drawPrimitive( D3DPRIMITIVETYPE primitiveType,
												   UINT startVertex,
												   UINT primitiveCount )
{
	return device_->DrawPrimitive( primitiveType, startVertex, primitiveCount );
}


INLINE uint32 RenderContext::drawPrimitiveUP( D3DPRIMITIVETYPE primitiveType,
													 UINT primitiveCount,
													 CONST void* pVertexStreamZeroData,
													 UINT vertexStreamZeroStride )
{
	return device_->DrawPrimitiveUP( primitiveType, primitiveCount,
									 pVertexStreamZeroData, vertexStreamZeroStride );
}


}

/*render_context.ipp*/
