/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TEXTURE_AGGREGATOR_HPP
#define TEXTURE_AGGREGATOR_HPP

#include "texture_manager.hpp"
#include <exception>

namespace Moo
{

class TextureAggregator
{
public:
	typedef void(*ResetNotifyFunc)(void);
	
	class InvalidArgumentError : public std::runtime_error
	{
	public:
		InvalidArgumentError(const std::string & message) :
			std::runtime_error(message) {}
	};

	class DeviceError : public std::runtime_error
	{
	public:
		DeviceError(const std::string & message) :
			std::runtime_error(message) {}
	};


	TextureAggregator(ResetNotifyFunc notifyFunc = NULL);
	~TextureAggregator();
	
	int addTile(
		BaseTexturePtr  tex, 
		const Vector2 & min, 
		const Vector2 & max);
		
	void getTileCoords(int id, Vector2 & min, Vector2 & max) const;
	
	void delTile(int id);
	
	void repack();
	bool tilesReset() const;
	
	DX::Texture * texture() const;
	const Matrix & transform() const;

	int minSize() const;
	void setMinSize(int minSize);

	int maxSize() const;
	void setMaxSize(int maxSize);

	int mipLevels() const;
	void setMipLevels(int mipLevels);

private:
	std::auto_ptr<struct TextureAggregatorPimpl> pimpl_;
};

} // namespace moo

#endif // TEXTURE_AGGREGATOR_HPP
