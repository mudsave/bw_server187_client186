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
#include "adjacent_chunk_set.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "WayPoint", 0 )

AdjacentChunkSet::AdjacentChunkSet()
{
}

/**
 * @param chunkID  the chunk id to get adjacencies for.
 */
bool AdjacentChunkSet::read(DataSectionPtr pChunkDir, const ChunkID& chunkID)
{
	DataSection::iterator chunkIter;
	DataSection::iterator boundaryIter;
	DataSectionPtr pChunk, pBoundary, pPortal, pAdjChunk;
	std::string name;
	
	pChunk = pChunkDir->openSection(chunkID + ".chunk");

	if(!pChunk)
	{
		ERROR_MSG("Failed to open chunk %s\n", chunkID.c_str());
		return false;
	}

	startChunk_ = chunkID;
	this->addChunk(chunkID);
	this->readChunk(pChunk);

	for(chunkIter = pChunk->begin();
		chunkIter != pChunk->end();
		chunkIter++)
	{
		pBoundary = *chunkIter;

		if(pBoundary->sectionName() != "boundary")
			continue;

		for(boundaryIter = pBoundary->begin();
			boundaryIter != pBoundary->end();
			boundaryIter++)
		{
			pPortal = *boundaryIter;

			if(pPortal->sectionName() != "portal")
				continue;

			std::string name = pPortal->readString("chunk");

			if(name == "heaven" || name == "earth" || name == "")
				continue;

			pAdjChunk = pChunkDir->openSection(name + ".chunk");

			if(!pAdjChunk)
			{
				ERROR_MSG("Failed to open adjacent chunk %s\n", name.c_str());
			}
			else if(!this->hasChunk(name))
			{
				this->addChunk(name);
				this->readChunk(pAdjChunk);
			}
		}
	}

	return true;
}

bool AdjacentChunkSet::readChunk(DataSectionPtr pChunk)
{
	DataSection::iterator chunkIter;
	DataSectionPtr pBoundary;
	Vector3 normal;
	Matrix transform;
	float d;

	for(chunkIter = pChunk->begin();
		chunkIter != pChunk->end();
		chunkIter++)
	{
		pBoundary = *chunkIter;

		transform = pChunk->readMatrix34("transform");

		if(pBoundary->sectionName() != "boundary")
			continue;

		if(pBoundary->readBool("portal/internal"))
		{
			// don't want internal ones.
			continue;
		}

		if(pBoundary->readString("portal/chunk") == "heaven" ||
		   pBoundary->readString("portal/chunk") == "earth" ||
		   pBoundary->readString("portal/chunk") == "extern")
		{
			// don't want heaven or earth (or extern).
			continue;
		}

		normal = pBoundary->readVector3("normal");
		d = pBoundary->readFloat("d");

		Vector3 ndtr = transform.applyPoint(normal * d);
		Vector3 ntr = transform.applyVector(normal);
		PlaneEq plane(ntr, ntr.dotProduct(ndtr));
		
		/*
		// 25/02/2003: JWD: Removed this as I cannot figure out what it is for.
		// Nick added it about this time last year, saying that it fixed y heights,
		// but it looks like a terrible hack to me and was stuffing things up.
		if(fabs(plane.normal().y - 1.0f) < 0.0001f)
		{
			plane = PlaneEq(plane.normal(), plane.d() - 5.0f);
		}
		*/

		this->addPlane(plane);
	}

	return true;
}

bool AdjacentChunkSet::hasChunk(const ChunkID& chunkID) const
{
	unsigned int i;

	for(i = 0; i < chunks_.size(); i++)
	{
		if(chunks_[i].chunkID == chunkID)
			return true;
	}

	return false;
}

void AdjacentChunkSet::addChunk(const ChunkID& chunkID)
{
	ChunkDef chunkDef;
	chunkDef.chunkID = chunkID;
	chunks_.push_back(chunkDef);
}

void AdjacentChunkSet::addPlane(const PlaneEq& planeEq)
{
	chunks_.back().planes.push_back(planeEq);
}


/**
 * @returns true if position in the chunk i, false otherwise.
 */
bool AdjacentChunkSet::testChunk(const Vector3& position, int i) const
{
	const ChunkDef& chunkDef = chunks_[i];

//	dprintf( "\tChecking chunk %s, nplanes %d\n",
//		chunkDef.chunkID.c_str(), chunkDef.planes.size() );

	for(unsigned int p = 0; p < chunkDef.planes.size(); p++)
	{
//		const Vector3 & n = chunkDef.planes[p].normal();
//		dprintf( "\t\tPlane (%f,%f,%f)>%f (dp %f)\n",
//			n.x, n.y, n.z, chunkDef.planes[p].d(), n.dotProduct(position) );
		if(!chunkDef.planes[p].isInFrontOf(position))
		{
			return false;
		}
	}

	// the point is in front of all boundary planes, so point is in the chunk.
	return true;
}


/**
 * Loops through the set of chunks testing to see if position is in any of them.
 * if it is then returns true and puts the corresponding chunk Id in chunkID.
 * else returns false.
 */
bool AdjacentChunkSet::test(const Vector3& position, ChunkID& chunkID) const
{	
//	dprintf( "Testing point (%f,%f,%f) against adj chunk set:\n",
//		position.x, position.y, position.z );

	// Test internal chunks first.
	
	for(unsigned int i = 0; i < chunks_.size(); i++)
	{
		if(chunks_[i].chunkID[8] == 'i' && testChunk(position, i))
		{
			chunkID = chunks_[i].chunkID;
			return true;
		}
	}

	for(unsigned int i = 0; i < chunks_.size(); i++)
	{
		if(chunks_[i].chunkID[8] != 'i' && testChunk(position, i))
		{
			chunkID = chunks_[i].chunkID;
			return true;
		}
	}

//	dprintf( "\tNot in any chunk\n" );
	return false;
}

const ChunkID& AdjacentChunkSet::startChunk() const
{
	return startChunk_;
}
