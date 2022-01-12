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
 *	The vertex type used to render branches and fronds.
 */
struct BranchVertex
{
	Vector3 position;
	Vector3 normal;
	FLOAT   texCoords[2];
	FLOAT   windIndex;
	FLOAT   windWeight;

	static DWORD fvf()
	{
		return
			D3DFVF_XYZ |
			D3DFVF_NORMAL |
			D3DFVF_TEXCOORDSIZE2(0) |
			D3DFVF_TEXCOORDSIZE2(1) |
			D3DFVF_TEX2;
	}
};

/**
 *	The vertex type used to render leaves.
 */
struct LeafVertex
{
	Vector3 position;
	Vector3 normal;
	FLOAT   texCoords[2];
	FLOAT   windInfo[2];
	FLOAT   rotInfo[2];
	FLOAT   extraInfo[2];
	FLOAT   geomInfo[2]; 
	FLOAT   pivotInfo[2];

	static DWORD fvf()
	{
		return
			D3DFVF_XYZ |
			D3DFVF_NORMAL |
			D3DFVF_TEXCOORDSIZE2(0) |
			D3DFVF_TEXCOORDSIZE2(1) |
			D3DFVF_TEXCOORDSIZE2(2) |
			D3DFVF_TEXCOORDSIZE2(3) |
			D3DFVF_TEXCOORDSIZE2(4) |
			D3DFVF_TEXCOORDSIZE2(5) |
			D3DFVF_TEX6;
	}
};

/**
 *	Holds data needed to render a tree part (branches, fronds or leaves).
 */
template< typename VertexType >
struct TreePartRenderData
{
	typedef VertexBuffer< VertexType >    VertexBuffer;
	typedef SmartPointer< VertexBuffer >  VertexBufferPtr;
	typedef SmartPointer< IndexBuffer >   IndexBufferPtr;
	typedef std::vector< IndexBufferPtr > IndexBufferVector;

	struct LodData
	{
		VertexBufferPtr   vertices;
		IndexBufferVector strips;
	};

	typedef std::vector< LodData > LodDataVector;

	BaseTexturePtr  texture;
	LodDataVector   lod;
};

// Render Data;
typedef TreePartRenderData< BranchVertex >    BranchRenderData;
typedef TreePartRenderData< LeafVertex >      LeafRenderData;
typedef TreePartRenderData< BillboardVertex > BillboardRenderData;


/**
 *	Trait class used to retrieve branch information in 
 *	both the getCommonRenderData and createPartRenderData functions.
 */
struct BranchTraits
{
	typedef BranchVertex VertexType;
	typedef TreePartRenderData< VertexType > RenderDataType;

	static std::string getTextureFilename(const CSpeedTreeRT::SMapBank & textureBank)
	{ 
		std::string fullName = textureBank.m_pBranchMaps[CSpeedTreeRT::TL_DIFFUSE];
		return BWResource::getFilename(fullName);
	}

	static const CSpeedTreeRT::SGeometry::SIndexed & 
		getGeometryData(CSpeedTreeRT & speedTree)
	{ 
		static CSpeedTreeRT::SGeometry geometry;
		speedTree.GetGeometry(geometry, SpeedTree_BranchGeometry); 
		return geometry.m_sBranches;
	}

	static int getLodLevelCount(const CSpeedTreeRT & speedTree)
		{ return speedTree.GetNumBranchLodLevels(); }
		
	static bool isSingleFaced() { return true; }
};

/**
 *	Trait class used to retrieve frond information in 
 *	both the getCommonRenderData and createPartRenderData functions.
 */
struct FrondTraits
{
	typedef BranchVertex VertexType;
	typedef TreePartRenderData< VertexType > RenderDataType;

	static std::string getTextureFilename(const CSpeedTreeRT::SMapBank & textureBank)
	{
		std::string fullName = textureBank.m_pCompositeMaps[CSpeedTreeRT::TL_DIFFUSE];
		return BWResource::getFilename(fullName);
	}

	static const CSpeedTreeRT::SGeometry::SIndexed & 
		getGeometryData(CSpeedTreeRT & speedTree)
	{ 
		static CSpeedTreeRT::SGeometry geometry;
		speedTree.GetGeometry(geometry, SpeedTree_FrondGeometry); 
		return geometry.m_sFronds;
	}

	static int getLodLevelCount(const CSpeedTreeRT & speedTree)
		{ return speedTree.GetNumFrondLodLevels(); }

	static bool isSingleFaced() { return true; }
};

/**
 *	Trait class used to retrieve leaf 
 *	information in the getCommonRenderData function.
 */
struct LeafTraits
{
	typedef LeafVertex VertexType;
	typedef TreePartRenderData< VertexType > RenderDataType;

	static std::string getTextureFilename(const CSpeedTreeRT::SMapBank & textureBank)
	{
		std::string fullName = textureBank.m_pCompositeMaps[CSpeedTreeRT::TL_DIFFUSE];
		return BWResource::getFilename(fullName);
	}
};

/**
 *	Trait class used to retrieve billboard 
 *	information in the getCommonRenderData function.
 */
struct BillboardTraits
{
	typedef BillboardVertex VertexType;
	typedef TreePartRenderData< VertexType > RenderDataType;

	static std::string getTextureFilename(const CSpeedTreeRT::SMapBank & textureBank)
	{
		std::string fullName = textureBank.m_pCompositeMaps[CSpeedTreeRT::TL_DIFFUSE];
		return BWResource::getFilename(fullName);
	}
};

/**
 *	Retrieves basic render data from a speed tree from a speed tree 
 *	object for a specific tree part. Uses the trait object for that.
 *
 *	@param	speedTree	speedtree object where to get the data from.
 *	@param	datapath	path where to look for aditional data files.
 *	@return				a new RenderDataType object.
 */
template< typename TreePartTraits >
typename TreePartTraits::RenderDataType getCommonRenderData(
	CSpeedTreeRT      & speedTree,
	const std::string & datapath)
{
	typename TreePartTraits::RenderDataType result;

	// get part texture
	CSpeedTreeRT::SMapBank textureBank;
	speedTree.GetMapBank(textureBank);

	std::string texFileName  = TreePartTraits::getTextureFilename(textureBank);
	std::string fullFileName = datapath + texFileName;
	
	bool success = (texFileName[0] != '\0');
	if (success)
	{
		result.texture = Moo::TextureManager::instance()->get(fullFileName);
		success = (result.texture->pTexture() != NULL);
	}
	if (!success)
	{
		std::string errorMsg = "SpeedTree: Cannot load texture file: ";
		throw std::runtime_error(errorMsg + fullFileName);
	}
	return result;
}

/**
 *	Creates a render data object to render the part of
 *	a tree specified by the trait template parameter.
 *
 *	@param	speedTree	speedtree object where to get the data from.
 *	@param	datapath	path where to look for aditional data files.
 *	@return				a new RenderDataType object.
 */
template< typename TreePartTraits >
typename TreePartTraits::RenderDataType createPartRenderData(
	CSpeedTreeRT      & speedTree,
	const std::string & datapath)
{
	TreePartTraits::RenderDataType result =
		getCommonRenderData< TreePartTraits >(speedTree, datapath);

	// extract geometry for all lod levels
	const CSpeedTreeRT::SGeometry::SIndexed & data = 
		TreePartTraits::getGeometryData(speedTree);

	// create and fill the index buffer for each LOD
	for (int lod=0; lod<data.m_nNumLods; ++lod)
	{
		// add new lod level
		typedef TreePartTraits::RenderDataType::LodData LodData;
		result.lod.push_back(LodData());
		LodData & lodData = result.lod.back();

		typedef VertexBuffer< TreePartTraits::VertexType > VBufferType;
		VBufferType * vertexBuffer = vertexBuffer = new VBufferType;

		// create index buffer for lod level
		int stripsCount = data.m_pNumStrips[lod];
		for (int strip=0; strip<stripsCount; ++strip)
		{
			int vCount = data.m_nNumVertices;
			
			lodData.strips.push_back(new IndexBuffer);			
			int iCount = data.m_pStripLengths[lod][strip];
			if (iCount > 0)
			{
				const int * iData = data.m_pStrips[lod][strip];
				bool result;
				if (TreePartTraits::isSingleFaced())
				{
					result = 
						lodData.strips.back()->reset(iCount) &&
						lodData.strips.back()->copy(iData);
				}
				else
				{
					// if double sided, copy indices 
					// twice into ibuffer. Second copy runs
					// backwards. To not repeat last index.
					int * zero = new int[iCount*2-1];
					
					int i = 0;
					for (; i<iCount-1; ++i)
					{
						zero[i] = iData[i];
						zero[iCount+i] = iData[iCount-2-i] + vCount;
					}
					zero[i] = iData[i];
					result = 
						lodData.strips.back()->reset(iCount*2-1);
						lodData.strips.back()->copy(zero);
						
					delete [] zero;
				}
				if (!result)
				{
					ERROR_MSG(
						"createPartRenderData: "
						"Could not create/copy index buffer\n");

					// abort this lod
					continue;
				}
			}

			// create vertex buffer for lod level
			if (lod == 0)
			{
				if (vCount > 0)
				{
					// if this should be double sided, copy vertices
					// twice into vbuffer (and flip normals of second copy)
					int facesCount = TreePartTraits::isSingleFaced() ? 1 : 2;
					if (vertexBuffer->reset(vCount*facesCount) &&
						vertexBuffer->lock())
					{
						VBufferType::iterator vbIt  = vertexBuffer->begin();
						VBufferType::iterator vbEnd = vertexBuffer->end();
						while (vbIt != vbEnd)
						{
							int distance = std::distance(vbIt, vbEnd);
							int i = (vCount * facesCount - distance) % vCount;

							// position (in speedtree, z is up)
							vbIt->position.x = data.m_pCoords[i * 3 + 0];
							vbIt->position.y = data.m_pCoords[i * 3 + 2];
							vbIt->position.z = data.m_pCoords[i * 3 + 1];

							vbIt->normal.x = data.m_pNormals[i * 3 + 0];
							vbIt->normal.y = data.m_pNormals[i * 3 + 2];
							vbIt->normal.z = data.m_pNormals[i * 3 + 1];

							// texcoords
							vbIt->texCoords[0] = data.m_pTexCoords[CSpeedTreeRT::TL_DIFFUSE][i * 2 + 0];
							vbIt->texCoords[1] = data.m_pTexCoords[CSpeedTreeRT::TL_DIFFUSE][i * 2 + 1];

							// wind animation
							vbIt->windIndex = data.m_pWindMatrixIndices[0][i];
							vbIt->windWeight = data.m_pWindWeights[0][i];

							++vbIt;
						}
						vertexBuffer->unlock();
					}
					else
					{
						ERROR_MSG(
							"createPartRenderData: "
							"Could not create/lock vertex buffer\n");
					}
				}
			}
			MF_ASSERT(vertexBuffer != NULL);
		}
		lodData.vertices = vertexBuffer;
	}

	return result;
}

/**
 *	Creates a render data object to render the leafs part of a tree.
 *
 *	@param speedTree	speedtree object where to get the data from.
 *	@param datapath	path where to look for aditional data files.
 */
LeafRenderData createLeafRenderData(
	CSpeedTreeRT      & speedTree,
	const std::string & filename,
	const std::string & datapath,
	int                 windAnglesCount,
	float             & leafRockScalar,
	float             & leafRustleScalar)
{
	LeafRenderData result =
		getCommonRenderData< LeafTraits >(speedTree, datapath);

	// extract geometry for all lod levels
	static CSpeedTreeRT::SGeometry geometry;
	speedTree.GetGeometry(geometry, SpeedTree_LeafGeometry);

	typedef CSpeedTreeRT::SGeometry::SLeaf SLeaf;
	typedef CSpeedTreeRT::SGeometry::SLeaf::SCard SCard;

	if (geometry.m_nNumLeafLods > 0)
	{
		const SLeaf & testData = geometry.m_pLeaves[0];
		const SCard & testCard = testData.m_pCards[0];
		if (testCard.m_pTexCoords[0] - testCard.m_pTexCoords[2] == 1.0 &&
			testCard.m_pTexCoords[5] - testCard.m_pTexCoords[3] == 1.0)
		{
			speedtreeError(filename.c_str(), 
				"Tree looks like it's not using composite maps.");
		}
	}

	for (int lod=0; lod<geometry.m_nNumLeafLods; ++lod)
	{
		typedef LeafTraits::RenderDataType::LodData LodData;
		result.lod.push_back(LodData());
		LodData & lodData = result.lod.back();

		typedef VertexBuffer< LeafVertex > VBufferType;
		lodData.vertices = new VBufferType;

		const SLeaf & data = geometry.m_pLeaves[lod];

        if (lod == 0)
        {
            leafRockScalar   = data.m_fLeafRockScalar;
            leafRustleScalar = data.m_fLeafRustleScalar;
        }

		int leafCount = data.m_nNumLeaves;
		if (leafCount > 0)
		{
			static const int vertexCount = 6;
			if (lodData.vertices->reset(leafCount*vertexCount) &&
				lodData.vertices->lock())
			{
				VBufferType::iterator vbIt = lodData.vertices->begin();
				for (int leaf=0; leaf<leafCount; ++leaf)
				{
					for (int v=0; v<vertexCount; ++v)
					{
						static const short vIdx[vertexCount] = { 0, 1, 2, 0, 2, 3 };

						// position (in speedtree, z is up)
						vbIt->position.x = data.m_pCenterCoords[leaf * 3 + 0];
						vbIt->position.y = data.m_pCenterCoords[leaf * 3 + 2];
						vbIt->position.z = data.m_pCenterCoords[leaf * 3 + 1];

						// normals
						vbIt->normal.x = data.m_pNormals[(leaf*4 + vIdx[v])*3 + 0];
						vbIt->normal.y = data.m_pNormals[(leaf*4 + vIdx[v])*3 + 2];
						vbIt->normal.z = data.m_pNormals[(leaf*4 + vIdx[v])*3 + 1];

						// wind info
						vbIt->windInfo[0] = data.m_pWindMatrixIndices[0][leaf];
						vbIt->windInfo[1] = data.m_pWindWeights[0][leaf];

						// retrieve leaf card										
						int cardIndex = data.m_pLeafCardIndices[leaf];
						const SCard & leafCard = data.m_pCards[cardIndex];

						// texture coords
						vbIt->texCoords[0] = leafCard.m_pTexCoords[vIdx[v]*2 + 0];
						vbIt->texCoords[1] = leafCard.m_pTexCoords[vIdx[v]*2 + 1];

						// rock and rustle info
						vbIt->rotInfo[0] = DEG_TO_RAD(leafCard.m_afAngleOffsets[0]);
						vbIt->rotInfo[1] = DEG_TO_RAD(leafCard.m_afAngleOffsets[1]);

						// extra info
						vbIt->extraInfo[0] = float(leaf % windAnglesCount);
						vbIt->extraInfo[1] = vIdx[v];

						// geometry info
						vbIt->geomInfo[0] = leafCard.m_fWidth;
						vbIt->geomInfo[1] = leafCard.m_fHeight;

						// pivot into
						vbIt->pivotInfo[0] = leafCard.m_afPivotPoint[0];
						vbIt->pivotInfo[1] = leafCard.m_afPivotPoint[1];

						++vbIt;
					}
				}
				MF_ASSERT(vbIt == lodData.vertices->end())
				lodData.vertices->unlock();
			}
			else
			{
				ERROR_MSG(
					"createLeafRenderData: "
					"Could not create/lock vertex buffer\n");
			}
		}
	}

	return result;
}

/**
 *	Creates a render data object to render the billboard part of a tree.
 *
 *	@param	speedTree	speedtree object where to get the data from.
 *	@param	datapath	path where to look for aditional data files.
 */
TreePartRenderData< BillboardVertex > createBillboardRenderData(
	CSpeedTreeRT      & speedTree,
	const std::string & datapath)
{
	TreePartRenderData< BillboardVertex > result =
		getCommonRenderData< BillboardTraits >(speedTree, datapath);

	typedef BillboardTraits::RenderDataType::LodData LodData;
	result.lod.push_back(LodData());
	LodData & lodData = result.lod.back();

	// generate billboard geometry
	speedTree.SetLodLevel(0);
	static CSpeedTreeRT::SGeometry geometry;
	speedTree.GetGeometry(geometry, SpeedTree_BillboardGeometry);
	speedTree.UpdateBillboardGeometry(geometry);

	// horizontal billboard
	typedef CSpeedTreeRT::SGeometry::SHorzBillboard SHorzBillboard;
	SHorzBillboard & dataHor = geometry.m_sHorzBillboard;
	bool drawHorizontalBB = 
		dataHor.m_pCoords    != NULL || 
		dataHor.m_pTexCoords != NULL;

	// 360 degrees billboards
	typedef CSpeedTreeRT::SGeometry::S360Billboard S360Billboard;
	S360Billboard & data360 = geometry.m_s360Billboard;
	const int bbCount = data360.m_nNumImages + (drawHorizontalBB ? 1 : 0);

	// create vertex buffer
	typedef VertexBuffer< BillboardVertex > VBufferType;
	lodData.vertices = new VBufferType;

	static const int vertexCount = 6;

	if (lodData.vertices->reset(bbCount*vertexCount) &&
		lodData.vertices->lock())
	{
		// fill vertex buffer	
		VBufferType::iterator vbIt = lodData.vertices->begin();
		Vector3 center(0, 0.5f * data360.m_fHeight, 0);
		for (int bb=0; bb<bbCount; ++bb)
		{
			// matching the billboard rotation with 
			// the tree's took a bit of experimentation. 
			// If, in future versions of speedtree, it 
			// stops matching, this is the place to tweak.
			Matrix bbSpace;
			bbSpace.setRotateY(-bb*(2*MATH_PI/data360.m_nNumImages) + (MATH_PI/2));

			static const short vIdx[vertexCount] = { 0, 1, 2, 0, 2, 3 };

			if (bb<(bbCount - (drawHorizontalBB ? 1 : 0)))
			{
				static const float xSignal[4]        = { +1.0f, -1.0f, -1.0f, +1.0f };
				static const float ySignal[4]        = { +1.0f, +1.0f, -0.0f, -0.0f };

				for (int v=0; v<vertexCount; ++v)
				{
					Vector3 tangVector = bbSpace.applyToUnitAxisVector(0);
					vbIt->position.x   = 0.5f * tangVector.x * xSignal[vIdx[v]] * data360.m_fWidth;
					vbIt->position.y   = ySignal[vIdx[v]] * data360.m_fHeight;
					vbIt->position.z   = 0.5f * tangVector.z * xSignal[vIdx[v]] * data360.m_fWidth;

					vbIt->lightNormal  = vbIt->position - center;
					vbIt->lightNormal.normalise();

					vbIt->alphaNormal[0] = -tangVector.z;
					vbIt->alphaNormal[1] = +tangVector.y;
					vbIt->alphaNormal[2] = +tangVector.x;

					vbIt->texCoords[0] = data360.m_pTexCoordTable[(bb)*8 + vIdx[v]*2 + 0];
					vbIt->texCoords[1] = data360.m_pTexCoordTable[(bb)*8 + vIdx[v]*2 + 1];
					++vbIt;
				}
			}
			else
			{
				for (int v=0; v<vertexCount; ++v)
				{
					vbIt->position.x   = dataHor.m_pCoords[vIdx[v]*3 + 0];
					vbIt->position.y   = dataHor.m_pCoords[vIdx[v]*3 + 2];
					vbIt->position.z   = dataHor.m_pCoords[vIdx[v]*3 + 1];

					vbIt->lightNormal.x = 0;
					vbIt->lightNormal.y = 1;
					vbIt->lightNormal.z = 0;

					vbIt->alphaNormal[0] = 0;
					vbIt->alphaNormal[1] = 1;
					vbIt->alphaNormal[2] = 0;

					vbIt->texCoords[0] = dataHor.m_pTexCoords[vIdx[v]*2 + 0];
					vbIt->texCoords[1] = dataHor.m_pTexCoords[vIdx[v]*2 + 1];
					++vbIt;
				}
			}	
		}
		MF_ASSERT(vbIt == lodData.vertices->end());
		lodData.vertices->unlock();
	}
	else
	{
		ERROR_MSG(
			"createBillboardRenderData: "
			"Could not create/lock vertex buffer\n");
	}

	return result;
}

/**
 *	Creates a string that uniquely identifies a tree loaded from 
 *	the given filename and generated with the provided seed number.
 *
 *	@param	filename	full path to the SPT file used to load the tree.
 *	@param	seed		seed number used to generate the tree.
 *	@return				the requested string.
 */
std::string createTreeDefName(const char* filename, uint seed)
{
	std::stringstream sstream;
	sstream << filename << "/" << seed;
	return sstream.str();
}


/**
 *	Sets up the SpeedWind object for this tree type. First, 
 *	try to load a tree specific setup file. If that can't be 
 *	found, tries to load the global wind setup file. If that 
 *	one is also missing, use default setup parameters.
 *
 *	@param	speedWind		the speedwind object to setup.
 *	@param	speedTree		the speedtree object that will use this speedwind.
 *	@param	datapath		data path where to look for the tree specific setup.
 *	@param	defaultWindIni	default ini file to use if tree specific not found.
 */
void setupSpeedWind(
	CSpeedWind2       & speedWind, 
	CSpeedTreeRT      & speedTree, 
	const std::string & datapath,
	const std::string & defaultWindIni)
{
	speedWind.Reset();
	speedTree.SetLeafRockingState(true);

	std::string resName  = datapath + "SpeedWind.ini";
	const std::string * iniFileList[2] = { &resName, &defaultWindIni };
	DataSectionPtr rootSection = BWResource::instance().rootSection();

	int i = 0;
	for (; i<2; ++i)
	{
		BinaryPtr windIniData = rootSection->readBinary(*iniFileList[i]);
		if (windIniData.exists())
		{
			BinaryInputBuffer bibuffer(windIniData);
			std::istream bistream(&bibuffer);
			if (speedWind.Load(bistream))
			{
				break;
			}
		}
	}
}
