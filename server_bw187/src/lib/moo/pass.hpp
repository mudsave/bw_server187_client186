/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PASS_HPP
#define PASS_HPP

namespace Moo
{

class Sampler;
typedef std::vector<Sampler*> Samplers;

class PixelOperation;
class VertexOperation;
class RenderData;

class Pass
{
public:
	Pass();
	~Pass();

	void	set( RenderData* pData );

	bool	load( DataSectionPtr pSection, const Samplers& samplers );
//	bool	save( DataSectionPtr pSection );


	typedef std::pair<D3DRENDERSTATETYPE, DWORD> RenderState;
	typedef std::vector< RenderState > RenderStates;

	void	addRenderState( D3DRENDERSTATETYPE state, DWORD value ) 
	{ renderStates_.push_back( std::make_pair(state, value ) );  }
	
	void	zBufferRead( bool state ) { zBufferRead_ = state; }
	bool	zBufferRead( ) const { return zBufferRead_; }
	void	zBufferWrite( bool state ) { zBufferWrite_ = state; }
	bool	zBufferWrite( ) const { return zBufferWrite_; }
	void	doubleSided( bool state ) { doubleSided_ = state; }
	bool	doubleSided( ) const { return doubleSided_; }
	void	fogged( bool state ) { fogged_ = state; }
	bool	fogged( ) const { return fogged_; }

	bool	fogOverride() const { return fogOverride_; }
	void	fogOverride( bool state ) { fogOverride_ = state; }
	DWORD	fogColour() const { return fogColour_; }
	void	fogColour( DWORD col ) { fogColour_ = col; }

	typedef std::pair< D3DTEXTURESTAGESTATETYPE, DWORD > State;
	typedef std::vector< State > States;
	struct TextureStage
	{
		TextureStage():
		sampler_( NULL )
		{}
		Sampler*	sampler_;
		States		states_;

		bool load( DataSectionPtr pSection, const Samplers& samplers );

		void set( uint32 stage );
	};

	typedef std::vector< TextureStage* > TextureStages;
	
	void	addTextureStage( TextureStage* ts ) { textureStages_.push_back( ts ); }
	TextureStages& textureStages() { return textureStages_; }

	VertexOperation*	vertexOperation() const { return pVertexOperation_; }
	void				vertexOperation( VertexOperation* pVertexOperation ) 
						{ pVertexOperation_ = pVertexOperation; }

	PixelOperation*		pixelOperation() const { return pPixelOperation_; }
	void				pixelOperation( PixelOperation* pPixelOperation ) 
						{ pPixelOperation_ = pPixelOperation; }
private:
	bool				zBufferRead_;
	bool				zBufferWrite_;
	bool				doubleSided_;
	bool				fogged_;
	bool				fogOverride_;
	DWORD				fogColour_;
	RenderStates		renderStates_;
	TextureStages		textureStages_;
	PixelOperation*		pPixelOperation_;
	VertexOperation*	pVertexOperation_;
};

}

#endif // PASS_HPP
