/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SPEEDTREE_RENDERER_HPP
#define SPEEDTREE_RENDERER_HPP

// BW Tech Hearders
#include "math/vector3.hpp"
#include "cstdmf/stdmf.hpp"
#include "cstdmf/smartpointer.hpp"

// Forward declarations
class Chunk;
class Matrix;
class BSPTree;
class BoundingBox;
class EnviroMinder;
class ShadowCaster;
typedef SmartPointer<class DataSection> DataSectionPtr;

namespace speedtree {

/**
 *	Single instance of a speedtree in the world. Each tree chunk item in the
 *	space will own an instance of SpeedTreeRenderer. SpeedTreeRenderer on its
 *	turn, will point to a shared TSpeedTreeType that holds the actual data
 *	needed to render the tree model.
 */ 
class SpeedTreeRenderer
{

public:
	SpeedTreeRenderer();
	~SpeedTreeRenderer();

	static void init(DataSectionPtr section);
	static void fini();
	
	void load(
		const char   * filename, 
		uint           seed, 
		const Chunk  * chunk);

	static void tick(float dTime);
	static void update();
	
	static void beginFrame(
		EnviroMinder * envMinder, 
		ShadowCaster * caster = NULL);
		
	static void flush();

	static void endFrame();

	void draw(const Matrix & transform);
	void resetTransform(const Chunk * chunk);

	const BoundingBox & boundingBox() const;
	const BSPTree & bsp() const;
	
	const char * filename() const;
	uint seed() const;

	static float lodMode(float newValue);
	static bool enviroMinderLighting(bool newValue);
	static bool drawTrees(bool newValue);
	
private:
	void drawRenderPass(const Matrix & transform);
	void drawShadowPass(const Matrix & transform);

	SmartPointer<struct TSpeedTreeType> treeType_;
	SmartPointer<class BillboardOptimiser> bbOptimiser_;
	int treeID_;
};

} // namespace speedtree

#endif // SPEEDTREE_RENDERER_HPP
