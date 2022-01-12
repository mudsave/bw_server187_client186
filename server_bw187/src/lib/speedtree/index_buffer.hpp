/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef INDEXBUFFER_HPP
#define INDEXBUFFER_HPP


#include "moo/render_context.hpp"
#include "moo/moo_dx.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/smartpointer.hpp"

namespace dxwrappers
{

class IndexBuffer : public SafeReferenceCount
{
public:
	IndexBuffer() :
		bufferSize_(0),
		ibuffer_(0)
	{}

	~IndexBuffer()
	{
		if (this->ibuffer_ != NULL)
		{
			this->ibuffer_->Release();
			this->ibuffer_ = NULL;
		}
	}

	bool reset(int bufferSize)
	{
		bool result = true;
		if (bufferSize == 0)
		{
			if (this->ibuffer_ != NULL)
			{
				MF_ASSERT(this->ibuffer_ != 0);
				this->ibuffer_->Release();
				this->ibuffer_ = NULL;
			}
		}
		else 
		{
			if (this->ibuffer_ != NULL)
			{
				this->reset(0);
			}
			MF_ASSERT(this->bufferSize_ == 0);
			int size = max(1, bufferSize);
			HRESULT hr = Moo::rc().device()->CreateIndexBuffer(
				bufferSize * sizeof(unsigned short), 
				D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, 
				D3DPOOL_MANAGED, &this->ibuffer_, NULL);	
				
			if (FAILED(hr))
			{
				this->ibuffer_ = NULL;
				result = false;
			}
		}
		this->bufferSize_ = bufferSize;
		return result;
	}

	bool copy(const unsigned short *source)
	{
		MF_ASSERT(this->ibuffer_ != 0)

		bool result = false;
		unsigned short *data;
        HRESULT hr = this->ibuffer_->Lock(0, 0, reinterpret_cast< void** >(&data), 0);
        if (SUCCEEDED(hr))
        {
			memcpy(data, source, this->bufferSize_ * sizeof(unsigned short));
			this->ibuffer_->Unlock();
			result = true;
		}
		return result;
	}

	bool copy(const int *source)
	{
		MF_ASSERT(this->ibuffer_ != 0)
		
		bool result = false;
		unsigned short *data;
        HRESULT hr = this->ibuffer_->Lock(0, 0, reinterpret_cast< void** >(&data), 0);
        if (SUCCEEDED(hr))
        {
			for (unsigned int i=0; i<this->bufferSize_; ++i)
			{
				*data = *source;
				++data;
				++source;
				
			}
			this->ibuffer_->Unlock();
			result = true;
		}
		return result;
	}

	int size() const
	{
		return this->bufferSize_;
	}

	bool isValid() const
	{
		return this->ibuffer_ != NULL && this->bufferSize_ > 0;
	}

	void activate() const
	{
		if (this->isValid())
		{
			Moo::rc().setIndices(this->ibuffer_);
		}
	}

	static void deactivate()
	{
		Moo::rc().setIndices(0);
	}

private:
	unsigned int      bufferSize_;
	DX::IndexBuffer * ibuffer_;

	// disallow copy
	IndexBuffer(const IndexBuffer &);
	const IndexBuffer & operator = (const IndexBuffer &);
};

} // namespace dxwrappers

#endif INDEXBUFFER_HPP
