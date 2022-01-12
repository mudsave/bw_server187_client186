/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef COM_OBJECT_WRAP_HPP
#define COM_OBJECT_WRAP_HPP

template <class ComObjectType>
struct ComObjectWrap
{
	typedef ComObjectType ComObject;

	ComObjectWrap()
	: pComObject_( NULL ) 
	{ 
	}

	ComObjectWrap( ComObject* pObject )
	: pComObject_( NULL ) 
	{ 
		pComObject( pObject ); 
	}

	ComObjectWrap(const ComObjectWrap<ComObjectType>& wrappedObject )
	: pComObject_( NULL ) 
	{ 
		pComObject( wrappedObject.pComObject() ); 
	}

	ComObjectWrap<ComObjectType>& operator=(int)
	{ 
		pComObject( NULL ); 
		return *this; 
	}

	ComObjectWrap<ComObjectType>& operator = ( const ComObjectWrap<ComObjectType>& X )
	{ 
		pComObject( X.pComObject() ); 
		return *this; 
	}

	~ComObjectWrap()
	{ 
		release(); 
	}

	friend bool operator == ( const ComObjectWrap<ComObjectType>& A, const ComObjectWrap<ComObjectType>& B )
	{ 
		return A.pComObject() == B.pComObject(); 
	}

	friend bool operator != ( const ComObjectWrap<ComObjectType>& A, int N )
	{ 
		return int( A.pComObject() ) != N; 
	}

	bool hasComObject() const
	{ 
		return pComObject() != 0; 
	}

	template <class RefID, class Ob>
	bool query(const RefID& ID, ComObjectWrap<Ob>& wrapper)
	{ 
		if ( !pComObject_ )
			return false;
		Ob* newObject = 0;
		if ( SUCCEEDED( pComObject_->QueryInterface( ID, (void **)&newObject) ) )
			wrapper.pComObject(newObject);
		return newObject != 0; 
	}

	void pComObject(ComObject *B)
	{ 
		if (B == pComObject_)
			return;
		release();
		pComObject_ = B; 
		if (pComObject_)
			pComObject_->AddRef(); 
		onComObjectChanged(); 
	}

	ComObject *pComObject() const
	{ 
		return pComObject_;
	}

	ComObject& operator*() const
	{ 
		return *pComObject();
	}

	ComObject *operator->() const
	{ 
		return pComObject(); 
	}

/*	operator ComObject*() const
	{
		return pComObject_;
	}*/

	ComObject** operator&()
	{ 
		return &pComObject_;
	}
protected:

	virtual void onComObjectChanged()
	{ 
	}

private:
	void release()
	{
		if (pComObject_)
			pComObject_->Release();
		pComObject_ = NULL;
	}

	ComObject* pComObject_;
};

#endif
