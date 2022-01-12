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
#include "cstdmf/memory_counter.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/bin_section.hpp"
#include "static_light_values.hpp"
#include "moo/render_context.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );

// -----------------------------------------------------------------------------
// Section: StaticLightValues
// -----------------------------------------------------------------------------


// the \0 is here to ensure the file will always be picked up as binary by cvs
const uint32 LIGHTING_FILE_HEADER = ('\0' << 24) | ('h' << 16) | ('g' << 8) | ('l');

/**
 *	Constructor.
 */
StaticLightValues::StaticLightValues(BinaryPtr pData)
: dirty_( true )
{
	init( pData );
}

/**
 *	Destructor.
 */
StaticLightValues::~StaticLightValues()
{
}

bool StaticLightValues::init( BinaryPtr pData )
{
	bool ret = false;
	if (pData.hasObject())
	{
		const D3DCOLOR* pColours = (const D3DCOLOR*) ((char*) pData->data() + sizeof(LIGHTING_FILE_HEADER));
		int nEntries = (pData->len() - sizeof(LIGHTING_FILE_HEADER)) / sizeof( D3DCOLOR );
		colours_.assign( pColours, pColours + nEntries );
		dirty_ = true;
		ret = true;
	}
	else
	{
		dirty_ = true;
		colours_.clear();
		vb_ = NULL;
	}
	return ret;
}

DX::VertexBuffer* StaticLightValues::vb()
{
	if (dirty_ && colours_.size())
	{
		DX::Device* pDev = Moo::rc().device();
		vb_ = NULL;

		DWORD usageFlag = D3DUSAGE_WRITEONLY | 
			(Moo::rc().mixedVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0);
		DX::VertexBuffer* vb = NULL;
		if (SUCCEEDED( pDev->CreateVertexBuffer( sizeof(D3DCOLOR) * colours_.size(),
			usageFlag,
			0,
			D3DPOOL_MANAGED,
			&vb, NULL )))
		{
            D3DCOLOR* vp;
			if (SUCCEEDED( vb->Lock( 0, 0, (void**)&vp, 0 ) ) )
			{
				memcpy( vp, &colours_.front(), sizeof(D3DCOLOR) * colours_.size() );
				vb->Unlock();

				vb_.pComObject( vb );
				dirty_ = false;
				vb_->Release();
			}
			else
			{
				CRITICAL_MSG( "StaticLightValues::vb: unable to lock vertex buffer\n" );
			}
		}
		else
		{
			CRITICAL_MSG( "StaticLightValues::vb: Unable to create vertex buffer with %d colour entries\n",
				colours_.size() );
		}
	}
	else if (!colours_.size())
		vb_ = NULL;

	return vb_.pComObject();
}

void StaticLightValues::deleteManagedObjects()
{
	dirty_ = true;
	vb_ = NULL;
}


bool StaticLightValues::save( DataSectionPtr binFile, const std::string& sectionName )
{
	MF_ASSERT(sectionName[sectionName.length() - 1] != '/');
	size_t tagIndex = sectionName.find_last_of('/');
	size_t modelIndex = sectionName.find_last_of('/', tagIndex - 1);
	size_t lightingIndex = sectionName.find_last_of('/', modelIndex - 1);
	size_t fileIndex = sectionName.find_last_of('/', lightingIndex - 1);
	MF_ASSERT(tagIndex != std::string::npos);
	MF_ASSERT(modelIndex != std::string::npos);
	MF_ASSERT(lightingIndex != std::string::npos);
	MF_ASSERT(fileIndex != std::string::npos);
	MF_ASSERT(sectionName.substr(lightingIndex, modelIndex - lightingIndex + 1) == "/lighting/");

	std::string fileNameShort = sectionName.substr(fileIndex + 1, lightingIndex - fileIndex - 1);
	MF_ASSERT(fileNameShort == binFile->sectionName());

	std::string modelName = sectionName.substr(modelIndex + 1, tagIndex - modelIndex - 1);
	std::string tag = sectionName.substr(tagIndex + 1);

	// set up some parents, so we can save
	DataSectionPtr lightingSection = binFile->openSection( "lighting", true );
	lightingSection->setParent(binFile);
	DataSectionPtr modelSection = lightingSection->openSection( modelName, true );
	modelSection->setParent(lightingSection);

	if ( !saveData( modelSection, tag ) )
		return false;

	return true;
}

bool StaticLightValues::saveData( DataSectionPtr section, const std::string& tag )
{
	if (!colours_.size())
	{
		CRITICAL_MSG( "StaticLightValues::saveData: Nothing to save in \"%s\"\n", tag.c_str());
		return false;
	}

	uint32 bufferLength = sizeof(LIGHTING_FILE_HEADER) + sizeof(D3DCOLOR) * colours_.size();
	char * dataBuffer = new char[bufferLength];
	memcpy( dataBuffer, &LIGHTING_FILE_HEADER, sizeof(LIGHTING_FILE_HEADER) );
	memcpy( dataBuffer + sizeof(LIGHTING_FILE_HEADER), 
			&colours_.front(), 
			sizeof(D3DCOLOR) * colours_.size() );

	if ( !section->writeBinary( tag, new BinaryBlock(dataBuffer, bufferLength) ) )
	{
		CRITICAL_MSG( "StaticLightValues::saveData: error while writing BinSection in \"%s\"\n", tag.c_str());
		return false;
	}

	// set parent pointer for saving
	DataSectionPtr tagSection = section->openSection( tag, false );
	tagSection->setParent(section);

	return true;
}

// static_light_values.cpp
