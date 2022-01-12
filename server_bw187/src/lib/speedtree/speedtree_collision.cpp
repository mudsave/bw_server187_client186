/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// Module Interface
#include "speedtree_collision.hpp"
#include "speedtree_config.hpp"

// BW Tech Hearders
#include "math/boundbox.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/md5.hpp"
#include "resmgr/multi_file_system.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/datasection.hpp"
#include "physics2/bsp.hpp"

// STD Headers
#include <map>
#include <stdexcept>

// Set SKIP_BSP_LOAD to 1 to prevent a cached
// bsp file to be loaded even if it's up-to-date
#define SKIP_BSP_LOAD 0

#if SPEEDTREE_SUPPORT
#include <SpeedTreeRT.h>
#endif // SPEEDTREE_SUPPORT

DECLARE_DEBUG_COMPONENT2("SpeedTree", 0)

namespace speedtree {

namespace { // anonymous

// Named Contants
const int   c_CollisionObjRes      = 8;
const float c_CoservativeBoxFactor = 0.5f;
const float c_PlaneEpsilon         = 0.1f;

#if SPEEDTREE_SUPPORT

BSPTreePtr loadBSPIfValid(
	BinaryPtr      bspData,
	CSpeedTreeRT * speedTree,
	bool         & o_emptyBSPAlreadyExists);

bool saveBSPWithMD5(
	BSPTree           & bspTree,
	const std::string & bspName,
	const MD5::Digest & md5Dig);

CSpeedTreeRT * acquireSpeedTree(
	CSpeedTreeRT * speedTree,
	const char   * filename);

void computeCollisionGeometry(
	CSpeedTreeRT     & speedTree,
	RealWTriangleSet & o_triangles,
	bool             & o_emptyBSP,
	MD5::Digest      & o_digest);

void createSphereCapTriagles(
	bool               upper,
	const float      * position,
	const Matrix     & rotation,
	const float      * dimensions,
	RealWTriangleSet & o_triangles,
	float              height = 0);

#endif // SPEEDTREE_SUPPORT

/**
 *	Helper class. Entry into the cache of BSPs.
 */
class CachedBSPTree : public SafeReferenceCount
{
public:
	CachedBSPTree(BSPTree * tree, const BoundingBox & bbox) :
		bspTree_(tree),
		boundingBox_(bbox)
	{}

	~CachedBSPTree()
	{
		delete this->bspTree_;
	}

	BSPTree * bspTree()
	{
		return this->bspTree_;
	}

	const BoundingBox & boundingBox()
	{
		return this->boundingBox_;
	}

private:
	BSPTree *   bspTree_;
	BoundingBox boundingBox_;
};

typedef SmartPointer< CachedBSPTree > CachedBSPTreePtr;
typedef std::map< std::string, CachedBSPTreePtr > SpeedTreeMap;
SpeedTreeMap s_treesCache;

} // namespace anonymous

/**
 *	Retrieves BSPTree for the specified SPT file.
 */
BSPTree * getBSPTree(const char * filename, BoundingBox & o_bbox)
{
	std::string name  = filename;
	BSPTree * bspTree = 0;
	SpeedTreeMap::iterator treeIt = s_treesCache.find(name);
	if (treeIt != s_treesCache.end())
	{
		bspTree = treeIt->second->bspTree();
		o_bbox.addBounds(treeIt->second->boundingBox());
	}
	else
	{
		BoundingBox bbox = BoundingBox::s_insideOut_;
		bspTree = createBSPTree(0, filename, bbox).release();
		o_bbox.addBounds(bbox);
		s_treesCache.insert(std::make_pair(name,
			CachedBSPTreePtr(new CachedBSPTree(bspTree, bbox))));
	}

	return bspTree;
}

/**
 *	Creates (or loads from file) a BSPTree for the specified SPT file.
 */
BSPTreePtr createBSPTree(
	CSpeedTreeRT * speedTree,
	const char   * filename,
	BoundingBox  & o_bbox)
{
	BinaryPtr bspData;
	BSPTreePtr bspTree;
	std::string bspName;
	bool emptyBSPAlreadyExists = false;

#if !SKIP_BSP_LOAD

#if SPEEDTREE_SUPPORT
	CSpeedTreeRT * speedTreeLocal = NULL;
#endif

	if (filename != 0)
	{
		std::string noExtension = BWResource::removeExtension(filename);
		bspName = noExtension + ".bsp2";
		if (BWResource::fileExists(bspName))
		{
			DataSectionPtr bspSect = BWResource::openSection(bspName);
			MF_ASSERT(bspSect.exists() != 0);
			bspData = bspSect->asBinary();
			MF_ASSERT(bspData.exists() != 0);

#if SPEEDTREE_SUPPORT
			if (!BWResource::isFileOlder(bspName, filename))
#endif // SPEEDTREE_SUPPORT
			{
				BSPTreePtr tree(new BSPTree);
				if (tree->load(bspData))
				{
					if (tree->empty())
					{
						emptyBSPAlreadyExists = true;
					}
					else
					{
						bspTree = tree;
					}
				}
				else
				{
					bspData = NULL;
					WARNING_MSG( "createBSP: Load failed\n" );
				}
			}
		}
	}
#if !SPEEDTREE_SUPPORT
	else
	{
		WARNING_MSG( "createBSP: No BSP file for %s\n", filename );
	}
#endif // !SPEEDTREE_SUPPORT
#endif // !SKIP_BSP_LOAD

#if !SPEEDTREE_SUPPORT
	// no need to delete speedTreeLocal here
	// cause speedtree support is not enabled

	if (bspTree.get() == 0)
	{
		if (!emptyBSPAlreadyExists)
		{
			std::string errorMsg = "Could not load bsp file for tree: ";
			throw std::runtime_error(errorMsg+filename);
		}
		else
		{
			return BSPTreePtr( NULL );
		}
	}
#else // !SPEEDTREE_SUPPORT
	if (bspData.exists() && bspTree.get() == 0 && !emptyBSPAlreadyExists)
	{
		// bsp file exists but it wasn't loaded because it's older
		// the spt file. loadBSPIfValid will load it, but only return
		// than it if the MD5 hash matches the one from the spt file.
		speedTreeLocal = acquireSpeedTree(speedTree, filename);
		bspTree = loadBSPIfValid(bspData, speedTreeLocal, emptyBSPAlreadyExists);
	}

	if (bspTree.get() == 0)
	{
		// if, at this point, we still don't have the bspTree,
		// rebuild it from the collision geometry in the spt file.
		speedTreeLocal = speedTreeLocal != NULL
			? speedTreeLocal
			: acquireSpeedTree(speedTree, filename);

		if (speedTreeLocal == NULL)
		{
			std::string errorMsg = "Could not load tree definition file: ";
			throw std::runtime_error(errorMsg+filename);
		}

		// create bsp
		MD5::Digest md5Dig;
		bool emptyBSP = false;
		RealWTriangleSet triangles;
		computeCollisionGeometry(*speedTreeLocal, triangles, emptyBSP, md5Dig);
		bspTree.reset(new BSPTree(triangles));

		// if tree BSP is actually empty, save an empty
		// bsp file, not the one that was returned.
		if (emptyBSP)
		{
			// but don't bother saving if the
			// empty bsp file already exists
			if (!emptyBSPAlreadyExists)
			{
				RealWTriangleSet tris;
				BSPTree emptyTree(tris);
				saveBSPWithMD5(emptyTree, bspName, md5Dig);
			}
		}
		else
		{
			saveBSPWithMD5(*bspTree, bspName, md5Dig);
		}
	}

#endif // !SPEEDTREE_SUPPORT

	// adjust bounding box
	MF_ASSERT(bspTree.get() != 0);
	if (!bspTree->empty()) 
	{ 
		const RealWTriangleSet & triangles = bspTree->triangles(); 
		RealWTriangleSet::const_iterator it = triangles.begin(); 
		RealWTriangleSet::const_iterator end = triangles.end(); 
		while (it != end) 
		{ 
			o_bbox.addBounds(it->v0()); 
			o_bbox.addBounds(it->v1()); 
			o_bbox.addBounds(it->v2()); 
			++it; 
		} 
	} 
	else 
	{ 
		o_bbox.addBounds(Vector3(0, 0, 0)); 
	} 

// don't mind deleting the tree if
// there is no support for speedtrees
#if SPEEDTREE_SUPPORT
	if (speedTree == 0)
	{
		delete speedTreeLocal;
	}
	else
	{
		MF_ASSERT(speedTreeLocal == NULL || speedTree == speedTreeLocal);
	}
#endif // SPEEDTREE_SUPPORT

	return bspTree;
}


namespace { // anonymous
#if SPEEDTREE_SUPPORT

/**
 *	Loads the provided bspTree is it's MD5 hash matches the
 *	one in the given speedtree.
 */
BSPTreePtr loadBSPIfValid(
	BinaryPtr      bspData,
	CSpeedTreeRT * speedTree,
	bool         & o_emptyBSPAlreadyExists)
{
	MF_ASSERT(bspData.exists());

	BSPTreePtr bsp(new BSPTree);
	bsp->load(bspData);
	bool result = true;

#if SPEEDTREE_SUPPORT
		// use md5 check only if
		// speedtree is enabled

		MD5::Digest bspDig;
		BinaryPtr bspDigData = bsp->getUserData(BSPTree::MD5_DIGEST);
		if (bspDigData.exists())
		{
			std::string bspQuoted  = bspDigData->cdata();
			bspDig.unquote(bspQuoted);
		}
		else
		{
			bspDig.unquote("12345678901234567890123456789012");
		}

		if (speedTree != NULL)
		{
			MD5 sptMD5;

			int collisionCount = speedTree->GetNumCollisionObjects();
			if (collisionCount > 0)
			{
				for (int i=0; i<collisionCount; ++i)
				{
					float data[10];
					CSpeedTreeRT::ECollisionObjectType type;
					speedTree->GetCollisionObject(i, type, &data[1], &data[4], &data[7]);
					data[0] = *(reinterpret_cast<float*>(&type));
					sptMD5.append(data, 10*sizeof(float));
				}
			}

			MD5::Digest sptDig;
			sptMD5.getDigest(sptDig);

			result = bspDig == sptDig;
		}
		else
		{
			// is speedtree is NULL, don't
			result = false;
		}
#endif // !SPEEDTREE_SUPPORT

	BSPTreePtr bspTree;
	if (result)
	{
		if (bsp->empty())
		{
			o_emptyBSPAlreadyExists = true;
		}
		else
		{
			bspTree = bsp;
		}
	}

	return bspTree;
}

/**
 *	Saves BSPTree to file, setting the provided
 *	MD5 digest as a user data entry into it.
 */
bool saveBSPWithMD5(
	BSPTree           & bspTree,
	const std::string & bspName,
	const MD5::Digest & md5Dig)
{
	std::string quote = md5Dig.quote();
	bspTree.setUserData(
		BSPTree::MD5_DIGEST,
		new BinaryBlock(quote.c_str(), quote.length()+1));

	return bspTree.save(bspName);
}

/**
 *	If the provided speedTree is not NULL, returns it. Otherwise, tries to
 *	loaded the one specified by filename. Returns NULL if loading fails.
 */
CSpeedTreeRT * acquireSpeedTree(CSpeedTreeRT * speedTree, const char * filename)
{
	if (speedTree != NULL)
	{
		return speedTree;
	}

	CSpeedTreeRT * tempSpeedTree;
	tempSpeedTree = new CSpeedTreeRT;
	const unsigned char* binData = 0;
	int binLen = 0;
	BinaryPtr bin = BWResource::instance().fileSystem()->readFile( filename );
	if ( !!bin )
	{
		binData = (unsigned char *)bin->cdata();
		binLen = bin->len();
	}
	if ( binData && binLen && !tempSpeedTree->LoadTree( binData, binLen ) )
	{
		speedTree = tempSpeedTree;
	}

	return speedTree;
}

/**
 *	Adds a triangle into the provided triangle set.
 */
inline void addTriangle(
    const Matrix        & xform,
    const Vector3       & p0,
    const Vector3       & p1,
    const Vector3       & p2,
    const Vector3       & p3,
    RealWTriangleSet    & triangles)
{
    // Transform the points:
    Vector3 w0 = p0 + xform.applyPoint(p1 - p0);
    Vector3 w1 = p0 + xform.applyPoint(p2 - p0);
    Vector3 w2 = p0 + xform.applyPoint(p3 - p0);
    // Test to see if the points are distinct enough to form a plane, and
    // if they are add them to the list of triangles:
    PlaneEq planeEq(w0, w1, w2, PlaneEq::SHOULD_NORMALISE);
    float len = planeEq.normal().length();
    if ((1.0f - c_PlaneEpsilon < len) && (len < 1.0f + c_PlaneEpsilon))
        triangles.push_back(WorldTriangle(w0, w1, w2, TRIANGLE_DOUBLESIDED));
}

/**
 *	Creates a box.
 */
void createBoxTriagles(
	const float      * position,
	const Matrix     & rotation,
	const float      * dimensions,
	RealWTriangleSet & o_triangles)
{
	const float & px = position[0];
	const float & py = position[2];
	const float & pz = position[1];

	const float w2 = dimensions[0]/2.0f;
	const float l2 = dimensions[1]/2.0f;
	const float & h = dimensions[2];
	const Vector3 p0(px, py, pz);
	const Vector3 b1(px-w2, py+0, pz-l2);
	const Vector3 b2(px-w2, py+0, pz+l2);
	const Vector3 b3(px+w2, py+0, pz+l2);
	const Vector3 b4(px+w2, py+0, pz-l2);
	const Vector3 t1(px-w2, py+h, pz-l2);
	const Vector3 t2(px-w2, py+h, pz+l2);
	const Vector3 t3(px+w2, py+h, pz+l2);
	const Vector3 t4(px+w2, py+h, pz-l2);
    addTriangle(rotation, p0, b3, b2, b1, o_triangles);
	addTriangle(rotation, p0, b1, b4, b3, o_triangles);
	addTriangle(rotation, p0, t1, t2, t3, o_triangles);
	addTriangle(rotation, p0, t3, t4, t1, o_triangles);
	addTriangle(rotation, p0, b1, b2, t2, o_triangles);
	addTriangle(rotation, p0, t2, t1, b1, o_triangles);
	addTriangle(rotation, p0, b2, b3, t3, o_triangles);
	addTriangle(rotation, p0, t3, t2, b2, o_triangles);
	addTriangle(rotation, p0, b3, b4, t4, o_triangles);
	addTriangle(rotation, p0, t4, t3, b3, o_triangles);
	addTriangle(rotation, p0, b4, b1, t1, o_triangles);
	addTriangle(rotation, p0, t1, t4, b4, o_triangles);
}

/**
 *	Creates the caps (poles) of a sphere.
 */
void createSphereCapTriagles(
	bool               upper,
	const float      * position,
	const Matrix     & rotation,
	const float      * dimensions,
	RealWTriangleSet & o_triangles,
	float              height)
{
	const int & res  = c_CollisionObjRes;

	// pivot point
	const Vector3 p0(position[0], position[2], position[1]);

	const float & px = p0.x;
	const float & py = p0.y + height;
	const float & pz = p0.z;
	const float & r = dimensions[0];
	
	for (int j=0; j<res; ++j)
	{
		const float angle1 = ((0.5f*MATH_PI/res) * j) - 0.5f*MATH_PI;
		const float angle2 = ((0.5f*MATH_PI/res) * (j+1)) - 0.5f*MATH_PI;
		const float r1 = cosf(angle1) * r;
		const float h1 = sinf(angle1) * r;
		const float r2 = cosf(angle2) * r;
		const float h2 = sinf(angle2) * r;
		for (int k=0; k<res; ++k)
		{
			const float angle1 = (2.0f*MATH_PI/res) * k;
			const float angle2 = (2.0f*MATH_PI/res) * ((k+1)%res);
			const float x1 = cosf(angle1);
			const float z1 = sinf(angle1);
			const float x2 = cosf(angle2);
			const float z2 = sinf(angle2);

			Vector3 b1, b2;
			if (upper)
			{
				b1 = Vector3(px+x1*r1, py-h1, pz+z1*r1);
				b2 = Vector3(px+x2*r1, py-h1, pz+z2*r1);
				const Vector3 t1(px+x1*r2, py-h2, pz+z1*r2);
				const Vector3 t2(px+x2*r2, py-h2, pz+z2*r2);
				addTriangle(rotation, p0, b2, t1, b1, o_triangles);
				addTriangle(rotation, p0, b2, t2, t1, o_triangles);
			}
			else
			{
				b1 = Vector3(px+x1*r1, py+h1, pz+z1*r1);
				b2 = Vector3(px+x2*r1, py+h1, pz+z2*r1);
				const Vector3 t1(px+x1*r2, py+h2, pz+z1*r2);
				const Vector3 t2(px+x2*r2, py+h2, pz+z2*r2);
				addTriangle(rotation, p0, b1, t1, b2, o_triangles);
				addTriangle(rotation, p0, t1, t2, b2, o_triangles);
			}
		}
	}
}

/**
 *	Computes the collision geometry for the provided speedtree.
 */
void computeCollisionGeometry(
	CSpeedTreeRT     & speedTree,
	RealWTriangleSet & o_triangles,
	bool             & o_emptyBSP,
	MD5::Digest      & o_digest)
{
	MD5 sptMD5;
	int collisionCount = speedTree.GetNumCollisionObjects();
	if (collisionCount > 0)
	{
		for (int i=0; i<collisionCount; ++i)
		{
			float position[3];
			float dimensions[3];
			float eulerAngles[3];
			CSpeedTreeRT::ECollisionObjectType type;
			speedTree.GetCollisionObject(i, type, position, dimensions, eulerAngles);

			// do md5 work
			float ftype = *(reinterpret_cast<float*>(&type));
			sptMD5.append(&ftype, sizeof(float));
			sptMD5.append(&position, sizeof(position));
			sptMD5.append(&dimensions, sizeof(dimensions));
			sptMD5.append(&eulerAngles, sizeof(eulerAngles));

			Matrix rotation, rotY, rotZ;
			rotation.setRotateX(DEG_TO_RAD(-eulerAngles[0]));
			rotY.setRotateY(DEG_TO_RAD(eulerAngles[2]));
			rotZ.setRotateZ(DEG_TO_RAD(-eulerAngles[1]));			
			rotation.preMultiply(rotZ);
			rotation.preMultiply(rotY);

			switch (type)
			{
			case CSpeedTreeRT::CO_BOX:
			{
				createBoxTriagles(position, rotation, dimensions, o_triangles);
				break;
			}
			case CSpeedTreeRT::CO_CAPSULE:
			{
				const int & res  = c_CollisionObjRes;
				const float & px = position[0];
				const float & py = position[2];
				const float & pz = position[1];

				const float & r = dimensions[0];
				const float & h = dimensions[1];
				const Vector3 p0(px, py, pz);
				const Vector3 bc(px, py+0, pz);
				const Vector3 tc(px, py+h, pz);
				for (int j=0; j<res; ++j)
				{
					const float angle1 = (2.0f*MATH_PI/res) * j;
					const float angle2 = (2.0f*MATH_PI/res) * ((j+1)%res);
					const float x1 = cosf(angle1) * r;
					const float z1 = sinf(angle1) * r;
					const float x2 = cosf(angle2) * r;
					const float z2 = sinf(angle2) * r;
					const Vector3 b1(px+x1, py+0, pz+z1);
					const Vector3 b2(px+x2, py+0, pz+z2);
					const Vector3 t1(px+x1, py+h, pz+z1);
					const Vector3 t2(px+x2, py+h, pz+z2);
					addTriangle(rotation, p0, b1, t1, b2, o_triangles);
					addTriangle(rotation, p0, t1, t2, b2, o_triangles);
				}
				createSphereCapTriagles(true, position, rotation, dimensions, o_triangles, h);
				createSphereCapTriagles(false, position, rotation, dimensions, o_triangles);
				break;
			}
			case CSpeedTreeRT::CO_SPHERE:
			{
				createSphereCapTriagles(true, position, rotation, dimensions, o_triangles);
				createSphereCapTriagles(false, position, rotation, dimensions, o_triangles);
				break;
			}
			}
		}
		o_emptyBSP = false;
	}
	else
	{
#ifdef EDITOR_ENABLED
			// no collision geometry exists, but we still
			// want to be able to select the tree in bigband.
			// Use the tree's bounding box as it's BSP tree.
			float bounds[6];
			speedTree.GetBoundingBox(bounds);
			const float height       = (bounds[5]-bounds[2]);
			const float width        = (bounds[3]-bounds[0])*c_CoservativeBoxFactor;
			const float length       = (bounds[4]-bounds[1])*c_CoservativeBoxFactor;
			const float dimensions[] = { width, length, height };
			const float position[]   = { 0, 0, 0 };
            Matrix rotation = Matrix::identity;
			createBoxTriagles(position, rotation, dimensions, o_triangles);
#endif // EDITOR_ENABLED
		o_emptyBSP = true;
	}
	sptMD5.getDigest(o_digest);
}

#endif // SPEEDTREE_SUPPORT
} // namespace anonymous
} // namespace speedtree
