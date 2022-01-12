/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VERTEXBUFFER_HPP
#define VERTEXBUFFER_HPP


#include "moo/moo_dx.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/smartpointer.hpp"

namespace dxwrappers
{

template< typename VertexType >
class VertexBuffer : public SafeReferenceCount
{
public:
	typedef VertexType * iterator;
	typedef const VertexType * const_iterator;

	VertexBuffer() : 
		bufferSize_(0),
		vbuffer_(NULL),
		data_(NULL),
		cdata_(NULL)
	{}

	~VertexBuffer()
	{
		if (this->vbuffer_ != NULL)
		{
			this->vbuffer_->Release();
			this->vbuffer_ = NULL;
		}
	}

	bool reset(int bufferSize = 0)
	{
		bool result = true;
		if (bufferSize == 0)
		{
			if (this->vbuffer_ != NULL)
			{
				MF_ASSERT(this->bufferSize_ != 0);
				this->vbuffer_->Release();
				this->vbuffer_ = NULL;
			}
		}
		else 
		{
			if (this->vbuffer_ != NULL)
			{
				this->reset(0);
			}
			MF_ASSERT(this->bufferSize_ == 0);
			int size = max(1, bufferSize);
			HRESULT hr = Moo::rc().device()->CreateVertexBuffer(
				size * sizeof(VertexType), 
				D3DUSAGE_WRITEONLY, VertexType::fvf(), 
				D3DPOOL_MANAGED, &this->vbuffer_, NULL);	

			if (FAILED(hr))
			{
				this->vbuffer_ = NULL;
				result = false;
			}
		}
		this->bufferSize_ = bufferSize;
		return result;
	}

	bool copy(const VertexType *source)
	{
		MF_ASSERT(this->vbuffer_ != 0)

		bool result = false;
		VertexType * data;
        HRESULT hr = this->vbuffer_->Lock(0, 0, reinterpret_cast< void** >(&data), 0);
        if (SUCCEEDED(hr))
        {
	        memcpy(data, source, this->bufferSize_ * sizeof(VertexType));
		    this->vbuffer_->Unlock();
		}
		return result;
	}

	bool lock()
	{
		MF_ASSERT(this->data_ == NULL);
		
		bool result = true;
        HRESULT hr = this->vbuffer_->Lock(0, 0, reinterpret_cast< void ** >(&this->data_), 0);
		if (FAILED(hr))
		{
			this->data_ = NULL;
			result = false;
		}
		return result;
	}

	bool lock() const
	{
		MF_ASSERT(this->cdata_ == NULL);

		bool result = true;
		VertexType *& cdata = const_cast<VertexType *&>(this->cdata_);
        HRESULT hr = this->vbuffer_->Lock(0, 0, reinterpret_cast< void ** >(&cdata), 0);
		if (FAILED(hr))
		{
			cdata = NULL;
			result = false;
		}
		return result;
	}

	VertexType & operator [] (uint index)
	{
		MF_ASSERT(this->data_ != NULL);
		MF_ASSERT(index <= this->bufferSize_);
        return this->data_[index];
	}

	const VertexType & operator [] (uint index) const
	{
		MF_ASSERT(this->cdata_ != NULL);
		MF_ASSERT(index <= this->bufferSize_);
        return this->cdata_[index];
	}

	iterator begin()
	{
		return this->data_;
	}

	const_iterator begin() const
	{
		return this->cdata_;
	}

	iterator end()
	{
		return this->data_ + this->bufferSize_;
	}

	const_iterator end() const
	{
		return this->cdata_ + this->bufferSize_;
	}

	void unlock()
	{
		MF_ASSERT(this->data_ != NULL);

        this->vbuffer_->Unlock();
		this->data_ = NULL;
	}

	void unlock() const
	{
		MF_ASSERT(this->cdata_ != NULL);

        this->vbuffer_->Unlock();

		VertexType *& cdata = const_cast<VertexType *&>(this->cdata_);
		cdata = NULL;
	}

	int size() const
	{
		return this->bufferSize_;
	}

	bool isValid() const
	{
		return this->vbuffer_ != NULL && this->bufferSize_ > 0;
	}

	void activate() const
	{
		if (this->isValid())
		{
			Moo::rc().device()->SetStreamSource(
				0, this->vbuffer_, 0, sizeof(VertexType));
		}
	}

	static void deactivate()
	{
		Moo::rc().device()->SetStreamSource(0, 0, 0, 0);
	}

private:
	unsigned int       bufferSize_;
	DX::VertexBuffer * vbuffer_;
	VertexType *       data_;
	const VertexType * cdata_;

	// disallow copy
	VertexBuffer(const VertexBuffer &);
	const VertexBuffer & operator = (const VertexBuffer &);
};

} // namespace dxwrappers

#endif VERTEXBUFFER_HPP
