/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef RESOURCE_REF_HPP
#define RESOURCE_REF_HPP


/**
 *	This helper class is a reference to some resource.
 *	It is half way between smart pointer and reference count.
 */
class ResourceRef
{
public:
	ResourceRef() : data_( NULL ), modRef_( NULL ) { }
	ResourceRef( void * data, void (*modRef)( void *, int ) ) :
		data_( data ), modRef_( modRef ) { if (data_) modRef_( data_, 1 ); }
	ResourceRef( const ResourceRef & other ) : data_( NULL ) { *this = other; }
	~ResourceRef() { if (data_) modRef_( data_, -1 ); }
	ResourceRef & operator=( const ResourceRef & other )
	{
		if (data_) modRef_( data_, -1 );
		modRef_ = other.modRef_;
		data_ = other.data_;
		if (data_) modRef_( data_, 1 );
		return *this;
	}

	operator bool() const { return !!data_; }

	static ResourceRef getIfLoaded( const std::string & id );
	static ResourceRef getOrLoad( const std::string & id );

private:
	void * data_;
	void (*modRef_)( void * data, int delta );

public:
	/**
	 *	This struct is a helper struct for loading different
	 *	kinds of resources depending on their extensions.
	 */
	struct ResourceFns
	{
		ResourceRef (*getIfLoaded_)( const std::string & id );
		ResourceRef (*getOrLoad_)( const std::string & id );

		static const ResourceFns & getForString( const std::string & id );
	};

	/**
	 *	Standard mod ref function
	 */
	template <class C> static void stdModRef( void * data, int delta )
	{
		C * pVal = (C*)data;
		if (delta > 0)	incrementReferenceCount( *pVal );
		else			decrementReferenceCount( *pVal );
	}
};


#endif // RESOURCE_REF_HPP