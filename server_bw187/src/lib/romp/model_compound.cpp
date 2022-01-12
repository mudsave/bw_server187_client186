/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "model_compound.hpp"
#include "moo/visual_compound.hpp"

DECLARE_DEBUG_COMPONENT2( "romp", 0 );

ModelCompoundInstance::ModelCompoundInstance()
: pModelCompound_(NULL)
{
}

ModelCompoundInstance::~ModelCompoundInstance()
{
	Moo::VisualCompound::MutexHolder mh;

	// Del all our visual compound lods.
	LodHolders::iterator it = lodHolders_.begin();
	while (it != lodHolders_.end())
	{
		it->second->del();
		++it;
	}

	// If we have a model compound remove ourselves from it
	if (pModelCompound_)
	{
		pModelCompound_->del( this );
	}
}

/**
 *	This method inits the compound from a model compound,
 *	creating transform holders for every visual compound.
 */
bool ModelCompoundInstance::init( ModelCompound* pModelCompound, 
		const Matrix& transform, uint32 batchCookie )
{
	const ModelCompound::LodCompounds& lcs = pModelCompound->lodCompounds();
	ModelCompound::LodCompounds::const_iterator it = lcs.begin();

	while (it != lcs.end())
	{
		Moo::VisualCompound* pvc = Moo::VisualCompound::get(it->second);

		MF_ASSERT_DEBUG( pvc );

		Moo::TransformHolder* pth = pvc->addTransform( transform, batchCookie );
		if (pth)
		{
			lodHolders_.push_back( std::make_pair( it->first, pth ) );
		}
		else
		{
			return false;
		}
		++it;
	}
	pModelCompound_ = pModelCompound;
	pModelCompound_->add( this );
	
	position_ = transform.applyToOrigin();
	yScale_ = max( 1.f, transform.applyToUnitAxisVector(1).length() );
	return true;
}

float ModelCompoundInstance::lodDistance()
{
	const Matrix& mooView = Moo::rc().view();
	float distance = position_.dotProduct(
		Vector3( mooView._13, mooView._23, mooView._33 ) ) + mooView._43;

	return (distance / yScale_) * Moo::rc().zoomFactor();
}

bool ModelCompoundInstance::draw()
{
	bool ret = false;
	if (pModelCompound_ && lodHolders_.size())
	{
		float lod = lodDistance();
		LodHolders::iterator it = lodHolders_.begin();
		bool lodFound = false;
		while (it != lodHolders_.end() && !lodFound)
		{
			if (it->first > lod || it->first == -1.f)
			{
				lodFound = true;
			}
			else
			{
				++it;
			}
		}
		if (it != lodHolders_.end())
		{
			if (it->second->pBatch())
			{
				it->second->draw();
				ret = true;
			}
		}
		else
		{
			ret = true;
		}
	}
	if (ret == false)
	{
		Moo::VisualCompound::MutexHolder mh;
		if (pModelCompound_)
			pModelCompound_->invalidate();
	}
	return ret;
}

ModelCompound::~ModelCompound()
{
	Moo::VisualCompound::MutexHolder mh;
	CompoundMap::iterator it = compoundMap_.begin();
	while (it != compoundMap_.end())
	{
		if (it->second == this)
		{
			compoundMap_.erase( it );
			break;
		}
		++it;
	}
	invalidate();
}

void ModelCompound::add( ModelCompoundInstance* instance )
{
	Moo::VisualCompound::MutexHolder mh;
	instances_.push_back( instance );
}

void ModelCompound::del( ModelCompoundInstance* instance )
{
	Moo::VisualCompound::MutexHolder mh;
	Instances::iterator it = std::find( instances_.begin(), instances_.end(), instance );
	if (it != instances_.end())
		instances_.erase(it);
	if (instances_.empty())
	{
		delete this;
	}
}

ModelCompoundInstance* ModelCompound::get( const std::string& name, 
	const Matrix& transform, uint32 batchCookie )
{
	Moo::VisualCompound::MutexHolder mh;
	ModelCompoundInstance* pResult = NULL;
	
	CompoundMap::iterator it = compoundMap_.find( name );

	if (it == compoundMap_.end())
	{
		ModelCompound* pSMC = new ModelCompound;
		if (!pSMC->init(name))
		{
			delete pSMC;
			pSMC = NULL;
		}
		it = compoundMap_.insert(std::make_pair(name, pSMC)).first;
	}

	if (it->second)
	{
		ModelCompoundInstance* pSMCI = new ModelCompoundInstance;
		if (pSMCI->init( it->second, transform, batchCookie ))
		{
			pResult = pSMCI;
		}
		else
		{
			delete pSMCI;
		}
	}

	return pResult;
}

void ModelCompound::invalidate()
{
	Moo::VisualCompound::MutexHolder mh;
	Instances::iterator it = instances_.begin();
	Instances::iterator end = instances_.end();
	while (it != end)
	{
		(*it)->pModelCompound_ = NULL;
		++it;
	}

	CompoundMap::iterator cmit = compoundMap_.begin();
	while (cmit != compoundMap_.end())
	{
		if (cmit->second == this)
		{
			cmit->second = NULL;
			break;
		}
		++cmit;
	}
}

ModelCompound::ModelCompound()
{
}

bool ModelCompound::init( const std::string& modelName )
{
	bool res = false;

	DataSectionPtr pModelSection = BWResource::openSection( modelName );
	if (pModelSection)
	{
		std::string visualName = pModelSection->readString( "nodelessVisual" );
		if (visualName.length())
		{
			Moo::VisualCompound* pVisualCompound = Moo::VisualCompound::get(
				visualName + ".visual" );
			if (pVisualCompound)
			{
				float lodDist = pModelSection->readFloat( "extent", -1.f );
				lodCompounds_.push_back( std::make_pair( lodDist, visualName + ".visual" ) );
				std::string parentName = pModelSection->readString( "parent" );
				if (parentName.length())
				{
					res = init( parentName + ".model" );
				}
				else
				{
					res = true;
				}
			}
		}
	}

	return res;
}

ModelCompound::CompoundMap ModelCompound::compoundMap_;
