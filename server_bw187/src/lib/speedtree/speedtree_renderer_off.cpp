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
#include "speedtree_renderer.hpp"
#include "speedtree_config.hpp"

#if !SPEEDTREE_SUPPORT // ------------------------------------------------------

// BW Tech Hearders
#include "cstdmf/debug.hpp"
#include "math/boundbox.hpp"
#include "physics2/bsp.hpp"
#include "resmgr/datasection.hpp"
#include <stdexcept>

DECLARE_DEBUG_COMPONENT2( "SpeedTree", 0 )

namespace speedtree {

struct TSpeedTreeType : public ReferenceCount {};
class BillboardOptimiser : public ReferenceCount {};

SpeedTreeRenderer::SpeedTreeRenderer() {}
SpeedTreeRenderer::~SpeedTreeRenderer() {}
void SpeedTreeRenderer::init( DataSectionPtr ) {}
void SpeedTreeRenderer::fini() {}
void SpeedTreeRenderer::tick( float ) {}
void SpeedTreeRenderer::update() {}
void SpeedTreeRenderer::beginFrame( EnviroMinder *, ShadowCaster * ) {}
void SpeedTreeRenderer::endFrame() {}
void SpeedTreeRenderer::draw( const Matrix &) {}
void SpeedTreeRenderer::resetTransform(const Chunk * chunk) {}
const char * SpeedTreeRenderer::filename() const { return "Disabled"; }
uint SpeedTreeRenderer::seed() const { return 1; }
float SpeedTreeRenderer::lodMode(float) { return 0; }
bool SpeedTreeRenderer::enviroMinderLighting(bool) { return false; }
bool SpeedTreeRenderer::drawTrees(bool) { return false; }

void SpeedTreeRenderer::load( const char * filename, uint seed, const Chunk * chunk )
{
	throw std::runtime_error( "SpeedTree support is disabled" );
}

const BoundingBox & SpeedTreeRenderer::boundingBox() const
{
	return BoundingBox::s_insideOut_;
}

const BSPTree & SpeedTreeRenderer::bsp() const
{
	static BSPTree bspTree;
	return bspTree;
};

} // namespace speedtree

#else  // !SPEEDTREE_SUPPORT ---------------------------------------------------

namespace
{
    int     noLinkWarning   = 0;    // stop the compiler complaining
}

#endif // !SPEEDTREE_SUPPORT ---------------------------------------------------
