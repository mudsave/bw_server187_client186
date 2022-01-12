/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	ThumbnailManager: Thumbnail generator class
 */

#ifndef THUMBNAIL_UTILS_HPP
#define THUMBNAIL_UTILS_HPP

#include "cstdmf/smartpointer.hpp"
#include <moo/render_target.hpp>

#include "atlimage.h"


/**
 *  Thumbnail Provider base class
 *	Derived classes must have a default constructor, or declare+implement
 *	the factory static themselves
 */
class ThumbnailProvider : public ReferenceCount
{
public:
	/**
	 *  This method is called by the thumbnail manager class to find out if
	 *  the provider supports this file type. If the provider returns true,
	 *  no other providers will be iterated on, so this provider should handle
	 *  the thumbnail.
	 *  @param file full path of the file
	 *  @return true if the file can be handled by the provider, false if not
	 */
	virtual bool isValid( const std::string& file ) = 0;
	
	/**
	 *  This method is called by the thumbnail manager class to find out if
	 *  the file needs a new new thumbnail to be created. If it returns true,
	 *  the manager will call the "create" method of the provider. If it
	 *  returns false, the manager will try to load directly a thumbnail from
	 *  the file matching the "thumb" parameter, so if the provider wishes to 
	 *  override the default thumbnail path and name, it can change it inside
	 *  this method by assigning the desired path to this parameter. That being
	 *  said, it is not recommended to change the default thumbnail name and/or
	 *  path.
	 *  @param file full path of the file
	 *  @param thumb full path of the thumbnail (in and out)
	 *  @return true to create a new thumbnail, false to load it from "thumb"
	 */
	virtual bool needsCreate( const std::string& file, std::string& thumb ) = 0;

	/**
	 *  This method is called by the thumbnail manager class to create a new
	 *  thumbnail for the file "file". A Moo render target is passed as a param
	 *  for the provider to render it's results. If this method returns true,
	 *  the Thumbnail manager class will save the render context to disk to a
	 *  file named as the string "thumb" passed to the "needsCreate" method.
	 *  @param file full path of the file
	 *  @param rt render target where the primitives will be rendered and later
	 *  saved to disk
	 */
	virtual bool create( const std::string& file, Moo::RenderTarget* rt ) = 0;
};
typedef SmartPointer<ThumbnailProvider> ThumbnailProviderPtr;



// Thumbnail manager class

class ThumbnailManager
{
public:
	static ThumbnailManager* instance();

	void registerProvider( ThumbnailProviderPtr provider );

	std::string postfix() { return postfix_; };
	std::string folder() { return folder_; };
	int size() { return size_; };
	COLORREF backColour() { return backColour_; };

	void postfix( const std::string& postfix ) { postfix_ = postfix; };
	void folder( const std::string& folder ) { folder_ = folder; };
	void size( int size ) { size_ = size; };
	void backColour( COLORREF backColour ) { backColour_ = backColour; };

	void create( const std::string& file, const std::string& postfix, CImage& img, int w, int h );
	void recalcSizeKeepAspect( int w, int h, int& origW, int& origH );
	bool renderImage( const std::string& file, Moo::RenderTarget* rt );

	// legacy methods
	void stretchImage( CImage& img, int w, int h, bool highQuality );
private:
	static ThumbnailManager* instance_;

	ThumbnailManager();

	std::vector<ThumbnailProviderPtr> providers_;

	std::string postfix_;
	std::string folder_;
	int size_;
	COLORREF backColour_;

	Moo::RenderTarget rt_;
	bool rtOk_;
};


/**
 *  Thumbnail provider factory
 */
class ThumbProvFactory
{
public:
	ThumbProvFactory( ThumbnailProviderPtr provider )
	{
		ThumbnailManager::instance()->registerProvider( provider );
	};
};

/**
 *	This macro is used to declare a class as a thumbnail provider. It is used
 *	to declare the factory functionality. It should appear in the declaration
 *	of the class.
 *
 *	Classes using this macro should also use the IMPLEMENT_THUMBNAIL_PROVIDER
 *  macro.
 */
#define DECLARE_THUMBNAIL_PROVIDER()										\
	static ThumbProvFactory s_factory_;

/**
 *	This macro is used to implement the thumbnail provider factory
 *  functionality.
 */
#define IMPLEMENT_THUMBNAIL_PROVIDER( CLASS )								\
	ThumbProvFactory CLASS::s_factory_( new CLASS() );



#endif // THUMBNAIL_UTILS_HPP
