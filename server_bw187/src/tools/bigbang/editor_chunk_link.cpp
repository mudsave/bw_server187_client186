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
#include "editor_chunk_link.hpp"
#include "editor_chunk_substance.ipp"
#include "editor_chunk_entity.hpp"
#include "selection_filter.hpp"
#include "station_link_operation.hpp"
#include "station_entity_link_operation.hpp"
#include "closest_obstacle_no_edit_stations.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/auto_config.hpp"
#include "romp/super_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "import/terrain_utils.hpp"
#include "moo/texture_manager.hpp"
#include "project/space_helpers.hpp"
#include <limits>

namespace
{
    const float         MAX_SEG_LENGTH          =       4.0f;   // largest space between segments
    const float         MIN_SEG_LENGTH          =       0.4f;   // minimum segment length
    const float         LINK_THICKNESS          =       0.1f;   // thickness of links
    const float         SEGMENT_SPEED           =       1.0f;   // speed texture moves along the segment   
    const float         TIME_FOR_RECALC         =       1.0f;   // time betweeen recalculation attempts for non-loaded chunks
    const float         NEXT_HEIGHT_SAMPLE      =       1.0f;   // sample height beyond current height per meter 
    const float         NEXT_HEIGHT_SAMPLE_MID  =       2.0f;   // sample height beyond mid estimate for mid point
    const float         MAX_SEARCH_HEIGHT       = 1000000.0f;   // max. search above height
    const float         AIR_THRESHOLD           =       1.0f;   // nodes above this height above the ground are in the air
    const float         BB_OFFSET               =       0.5f;   // amount to offset bounding box

    AutoConfigString    linkTexture             = "editor/linkTexture";
    AutoConfigString    entityLinkTexture       = "editor/entityLinkTexture";
    AutoConfigString    linkShader              = "editor/linkShader";    

    /**
     *  This is used to hit-test chunk links.
     */
    struct EditChunkLinkObstacle : public ChunkObstacle
    {
    public:
        /**
         *  Constructor.
         *
         *  @param link         The link to hit test.
         *  @param transform    The local to world transform.
         *  @param bb           The bounding box.  WARNING  This bounding box's
         *                      address is used by ChunkObstacle later!
         */
        EditChunkLinkObstacle
        (
            EditorChunkLinkPtr  link,
            Matrix              const &transform, 
            BoundingBox         const *bb,
			Vector3				startPt,
			Vector3				endPt
        ) :
            ChunkObstacle(transform, bb, link),
            link_(link),
			startPt_(startPt),
			endPt_(endPt)
        {
        }
        
        /**
         *  Do we have a collision?
         *
         *  @param source       The source vector.
         *  @param extent       The extent vector.
         *  @param state        The collision state.
         *
         *  @return             True if more collisions should be tested.
         */
        /*virtual*/ bool 
        collide
        ( 
            Vector3             const &source, 
            Vector3             const &extent,
            CollisionState      &state 
        ) const
        {
            Vector3 dir = extent - source;
            WorldTriangle wt;
            float dist = link_->collide(source, dir, wt);            
            if (dist == std::numeric_limits<float>::max())
                return false;
            dist = state.sTravel_ + (state.eTravel_- state.sTravel_)*dist;
	        if (state.onlyLess_ && dist > state.dist_)
                return false;
	        if (state.onlyMore_ && dist < state.dist_) 
                return false;            
            int say = state.cc_(*this, wt, dist);
            if ((say & 3) != 3) 
                state.dist_ = dist;
	        if (say == 0) 
                return true;
	        state.onlyLess_ = !(say & 2);
	        state.onlyMore_ = !(say & 1);
	        return false;
        }

        /**
         *  Do we have a collision?
         *
         *  @param source       The source triangle.
         *  @param extent       The extent vector.
         *  @param state        The collision state.
         *
         *  @return             True if more collisions should be tested.
         */
        /*virtual*/ bool 
        collide
        ( 
            WorldTriangle       const &source, 
            Vector3             const &extent,
		    CollisionState      &state 
        ) const
        {
            bool ok = false;
            ok |= collide(source.v0(), extent, state);
            ok |= collide(source.v1(), extent, state);
            ok |= collide(source.v2(), extent, state);
            return ok;
        }

        /**
         *  Returns the end points of the link.
         *
		 *	@param startPt	Returns start point of the link in absolute coords.
		 *	@param endPt	Returns end point of the link in absolute coords.
         */
		void endPoints( Vector3& startPt, Vector3& endPt )
		{
			startPt = startPt_;
			endPt = endPt_;
		}

    private:
        EditorChunkLinkPtr      link_;
		Vector3					startPt_;
		Vector3					endPt_;
    };


    /**
     *  This class adds ChunkObstacle objects to the space for the chunks
	 *	that this link intersects.
     */
	class ChunkLinkObstacle : public ChunkModelObstacle
	{
	public:
		ChunkLinkObstacle( Chunk & chunk ) :
			ChunkModelObstacle( chunk )
		{
		}

		~ChunkLinkObstacle()
		{
		}

		static Instance<ChunkLinkObstacle> instance;

	protected:

		typedef std::set< ChunkSpace::Column * > ColumnSet;

		/**
		 *	This protected method inserts all relevant columns in the 'columns'
		 *	return parameter using the link's ChunkObstacle object 'cso'.
		 *
		 *	@param cso		The link's ChunkObstacle object
		 *	@param columns	Return paramenter, relevant columns
		 *	@param adding	True if adding to space, false otherwise
		 */
		void getColumns( ChunkObstacle & cso, ColumnSet& columns, bool adding )
		{
			// get the world-aligned bounding box that contains the link's BB.
			BoundingBox bbTr = cso.bb_;
			bbTr.transformBy( cso.transform_ );
			Vector3 startPt;
			Vector3 endPt;
			static_cast<EditChunkLinkObstacle&>( cso ).endPoints( startPt, endPt );

			// transform to grid coords
			int minX = int( floorf( bbTr.minBounds().x / GRID_RESOLUTION ) );
			int minZ = int( floorf( bbTr.minBounds().z / GRID_RESOLUTION ) );
			int maxX = int( floorf( bbTr.maxBounds().x / GRID_RESOLUTION ) );
			int maxZ = int( floorf( bbTr.maxBounds().z / GRID_RESOLUTION ) );

			// for each grid area inside the BB, test to see if the link
			// touches it, and if so, add it to the columns.
			Vector2 line( endPt.x - startPt.x, endPt.z - startPt.z );

			// If the line is zero length, simply return
			if (line.lengthSquared() == 0.0f)
				return;

			for (int x = minX; x <= maxX; ++x)
			{
				for (int z = minZ; z <= maxZ; ++z)
				{
					// find out if the link line is inside the chunk in the
					// grid pos (x,y), by comparing the signs of the cross
					// products 
					Vector2 wpos( x * GRID_RESOLUTION, z * GRID_RESOLUTION );
					Vector2 corners[4] = {
						wpos,
						wpos + Vector2( GRID_RESOLUTION, 0.0f ),
						wpos + Vector2( 0.0f, GRID_RESOLUTION ),
						wpos + Vector2( GRID_RESOLUTION, GRID_RESOLUTION )
					};

					bool linkIn = false;
					int lastSign = 0;
					for ( int i = 0; i < 4; ++i )
					{
						Vector2 cornerVec( corners[i].x - startPt.x, corners[i].y - startPt.z );
						float cross = line.crossProduct( cornerVec );
						if ( lastSign == 0 )
						{
							if ( cross >= 0 )
								lastSign = 1;
							else
								lastSign = -1;
						}
						else
						{
							if ( ( cross >= 0 && lastSign == -1 ) ||
								 ( cross < 0 && lastSign == 1 ) )
							{
								// sign changed, so should add this column
								linkIn = true;
								break;
							}
						}
					}

					if ( linkIn )
					{
						// Should add this column, so do it.
						Vector3 pt(
							(x + 0.5f) * GRID_RESOLUTION,
							0.0f,
							(z + 0.5f) * GRID_RESOLUTION );

						ChunkSpace::Column* pColumn =
							pChunk_->space()->column( pt, adding );

						if ( pColumn != NULL )
							columns.insert( pColumn );
					}
				}
			}

			if (columns.empty() && adding)
			{
				ERROR_MSG(
					"ChunkLinkObstacle::getColumns: Link is not inside the space - "
					"min = (%.1f, %.1f, %.1f). max = (%.1f, %.1f, %.1f)\n",
					bbTr.minBounds().x, bbTr.minBounds().y, bbTr.minBounds().z,
					bbTr.maxBounds().x, bbTr.maxBounds().y, bbTr.maxBounds().z );
			}
		}

		/**
		 *	This protected method adds the obstacle to all its relevant comumns
		 *
		 *	@param cso		The link's ChunkObstacle object
		 *	@return			Number of columns added to.
		 */
		virtual int addToSpace( ChunkObstacle & cso )
		{
			int colCount = 0;

			// find what columns we need to add to
			ColumnSet columns;

			getColumns( cso, columns, true );

			// and add it to all of them
			for (ColumnSet::iterator it = columns.begin(); it != columns.end(); it++)
			{
				MF_ASSERT( &**it );	// make sure we can reach all those we need to!
				if (*it)
				{
					(*it)->addObstacle( cso );
					++colCount;
				}
			}

			return colCount;
		}

		/**
		 *	This protected method removes the obstacle from all its relevant
		 *	comumns
		 *
		 *	@param cso		The link's ChunkObstacle object
		 */
		virtual void delFromSpace( ChunkObstacle & cso )
		{
			ColumnSet columns;

			getColumns( cso, columns, false );

			// and add it to all of them
			for (ColumnSet::iterator it = columns.begin(); it != columns.end(); it++)
			{
				if ( *it )
					(*it)->stale();
			}
		}
	};
	ChunkCache::Instance<ChunkLinkObstacle> ChunkLinkObstacle::instance;
}


/*static*/ bool EditorChunkLink::s_enableDraw_ = true;


/**
 *  EditorChunkLink constructor.
 */
EditorChunkLink::EditorChunkLink() :
    yOffset_(0.5f),
    totalTime_(0.0f),
    minY_(0.0f),
    maxY_(0.0f),
    needRecalc_(false),
    recalcTime_(0.0f),
    midY_(0.0f)
{
    lastStart_.x = lastStart_.y = lastStart_.z =
        lastEnd_.x = lastEnd_.y = lastEnd_.z =
        std::numeric_limits<float>::max();

    transform_.setIdentity();

    HRESULT hr = S_OK;

    // Load the link texture:
    std::string linkTextureFile = 
        BWResource::resolveFilename(linkTexture);
    hr = 
        D3DXCreateTextureFromFileEx
        (
            Moo::rc().device(),
            linkTextureFile.c_str(),
            D3DX_DEFAULT,
            D3DX_DEFAULT,
            1,              // no mipmaps
            0,              // not a render target
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            D3DX_FILTER_POINT,
            D3DX_FILTER_NONE ,
            0,              // no colour-key
            NULL,
            NULL,
            &linkTexture_ 
        );
	MF_ASSERT(SUCCEEDED(hr));

    // Load the entity-node texture:
    std::string entityTextureFile = 
        BWResource::resolveFilename(entityLinkTexture);
    hr = 
        D3DXCreateTextureFromFileEx
        (
            Moo::rc().device(),
            entityTextureFile.c_str(),
            D3DX_DEFAULT,
            D3DX_DEFAULT,
            1,              // no mipmaps
            0,              // not a render target
            D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED,
            D3DX_FILTER_POINT,
            D3DX_FILTER_NONE ,
            0,              // no colour-key
            NULL,
            NULL,
            &entityTexture_ 
        );
    MF_ASSERT(SUCCEEDED(hr));

    meshEffect_ = new Moo::EffectMaterial();
    meshEffect_->initFromEffect(linkShader);
}


/**
 *  EditorChunkLink destructor.
 */
EditorChunkLink::~EditorChunkLink()
{
    delete meshEffect_; meshEffect_ = NULL;
    linkTexture_  ->Release(); linkTexture_   = NULL;
    entityTexture_->Release(); entityTexture_ = NULL;
}


/**
 *  This draws a EditorChunkLink.
 */
/*virtual*/ void EditorChunkLink::draw()
{
    if ( edShouldDraw() )
    {
        if (startItem() == NULL || endItem() == NULL)
            return;

        Moo::RenderContext *rc = &Moo::rc();

        DX::Texture *texture = 
            direction() != DIR_NONE ? linkTexture_ : entityTexture_;

        ChunkLinkSegment::beginDrawSegments
        (
            *rc, 
            texture, 
            meshEffect_,
            totalTime_,
            SEGMENT_SPEED,
            direction()
        );

        if (meshes_.size() != 0)
        {
            ChunkLinkSegment::draw
            (
                *rc, 
                vertexBuffer_, 
                indexBuffer_, 
                meshes_.size()
            );
        }

        ChunkLinkSegment::endDrawSegments
        (
            *rc, 
            texture, 
            meshEffect_,
            totalTime_,
            SEGMENT_SPEED,
            direction()
        );

        // The following is useful for debugging sometimes
        //for (size_t i = 0; i < meshes_.size(); ++i)
        //{
        //    meshes_[i]->drawTris(*rc);
        //}  
    }
}


/**
 *  This gets the section name of a link.
 *
 *  @return             "link".
 */
/*virtual*/ char const *EditorChunkLink::sectName() const
{
    return "link";
}


/**
 *  This gets the draw flag for a link.
 *
 *  @return             "render/drawEntities".
 */
/*virtual*/ const char *EditorChunkLink::drawFlag() const
{
    return "render/drawEntities";
}


/**
 *  This gets the representative model (there is none).
 *
 *  @return             NULL.
 */
/*virtual*/ ModelPtr EditorChunkLink::reprModel() const
{
    return NULL;
}


/**
 *  This gets the local to world transform.
 *
 *  @return             The local to world transform.
 */
/*virtual*/ const Matrix & EditorChunkLink::edTransform()
{
    return transform_;
}


/**
 *  This gets called when the link is moved to a new chunk.
 *
 *  @param newChunk     The new chunk.  NULL denotes removal from the old 
 *                      chunk.
 */
/*virtual*/ void EditorChunkLink::toss(Chunk *newChunk)
{
    if (chunk() != NULL)
    {
        ChunkLinkObstacle::instance(*chunk()).delObstacles(this);
    }    

    removeFromLentChunks();

	ChunkLink::toss(newChunk);

	if (newChunk != NULL)
	{
	    addAsObstacle();
	}   
}


/**
 *  This is called to update the link.
 *
 *  @param dtim         The time since this function was last called.
 */
/*virtual*/ void EditorChunkLink::tick(float dtime)
{
    totalTime_ += dtime;

    Vector3 startPt, endPt;
    bool ok = getEndPoints(startPt, endPt, true);
    if (!ok)
        return;

    startPt.y += yOffset_;
    endPt  .y += yOffset_;

    recalcTime_ += dtime;

    bool recalc = false;
    recalc |= (startPt != lastStart_ || endPt != lastEnd_);
    recalc |= needRecalc_ && recalcTime_ > TIME_FOR_RECALC;
    if (recalc)
    {
        recalcMesh(startPt, endPt);	
    }
}


/**
 *  This function adds the link as an obstacle.
 */ 
/*virtual*/ void EditorChunkLink::addAsObstacle()
{
	Matrix world(chunk()->transform());
	world.preMultiply(edTransform());

	// Get the BB in absolute coordinates, because minY and maxY are in
	// absolute coordinates as well.
    Vector3 pt1, pt2;
    getEndPoints(pt1, pt2, true);

    bb_ = BoundingBox
		(
	        Vector3
            (
                std::min(pt1.x, pt2.x) - BB_OFFSET, 
                minY_                  - BB_OFFSET, 
                std::min(pt1.z, pt2.z) - BB_OFFSET
            ),
		    Vector3
            (
                std::max(pt1.x, pt2.x) + BB_OFFSET, 
                maxY_                  + BB_OFFSET, 
                std::max(pt1.z, pt2.z) + BB_OFFSET
            )
		);

	// Bring the BB back to local coordinates for the obstacle to work.
    Matrix m = chunk()->transform();
    m.invert();
	bb_.transformBy( m );

    ChunkLinkObstacle::instance(*chunk()).addObstacle
    (
	    new EditChunkLinkObstacle(this, world, &bb_, pt1, pt2 )
    );
}


/**
 *  This function gets the list of right-click menu options.
 *
 *  @param path         The path of the item.
 *
 *  @return             The list of right-click menu options.
 */
/*virtual*/ std::vector<std::string> 
EditorChunkLink::edCommand( std::string const &/*path*/ ) const
{ 
    std::vector<std::string> commands;

    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();
    // Nodes at each end?
    if (start != NULL && end != NULL)
    {    
        commands.push_back("Disconnect");
        commands.push_back("Swap direction (this link)");
        commands.push_back("Both directions (this link)");
        commands.push_back("Swap direction (run of links)");
        commands.push_back("Both directions (run of links)");
        commands.push_back("Split");
#ifdef _DEBUG
        commands.push_back("Validate");
#endif
    }
    // A node at one end and an entity at the other?
    else if (start != NULL || end != NULL)
    {
        commands.push_back("Delete");
    }
    return commands;
}


/**
 *  This is called when the user does a right-click command.
 *
 *  @param path         The path of the object.
 *  @param index        The index of the command.
 *
 *  @return             True if the command was done.
 */
/*virtual*/ bool 
EditorChunkLink::edExecuteCommand
( 
    std::string     const &path, 
    std::vector<std::string>::size_type index 
)
{
    ScopedSyncMode scopedSyncMode;

    loadGraph(); // force graph's chunks to be in memory

    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode  ();

    // Node at each end case:
    if (start != NULL && end != NULL)
    {
        switch (index)
        {
        case 0: deleteCommand    (); return true;
        case 1: swapCommand      (); return true;
        case 2: bothDirCommand   (); return true;
        case 3: swapRunCommand   (); return true;
        case 4: bothDirRunCommand(); return true;
        case 5: splitCommand     (); return true;
        case 6: validateCommand  (); return true;
        default:                     return false;
        }
    }
    // Node, entity case:
    else if (start != NULL || end != NULL)
    {
        switch (index)
        {
        case 0: deleteLinkCommand(); return true;
        default:                     return false;
        }
    }
    return false;
}


/**
 *  This function is the collision test for EditorChunkLink.
 *
 *  @param source       The source vector.
 *  @param dir          The direction.
 *  @param wt           The collision triangle.
 *
 *  @return             std::numeric_limits<float>::max() for no collision,
 *                      the t-value of the collision otherwise.
 */
float
EditorChunkLink::collide
(
    Vector3         const &source, 
    Vector3         const &dir,
    WorldTriangle   &wt
) const
{
    // Convert from local coordinates to global coordinates:
    Vector3 tsource = chunk()->transform().applyPoint(source);
    Vector3 tdir = chunk()->transform().applyVector(dir);

    float dist = std::numeric_limits<float>::max();

    for (size_t i = 0; i < meshes_.size(); ++i)
    {
        meshes_[i]->intersects(tsource, tdir, dist, wt);
    }

	// Bring it back to local coords
	wt = WorldTriangle(
			chunk()->transformInverse().applyPoint( wt.v0() ),
			chunk()->transformInverse().applyPoint( wt.v1() ),
			chunk()->transformInverse().applyPoint( wt.v2() ) );

    return dist;
}


/**
 *  This function returns the midpoint of the link, taking into account the
 *  height of the terrain.
 * 
 *  @param chunk            The chunk that the midpoint is in.
 *
 *  @return                 The position of the midpoint.
 */ 
Vector3 EditorChunkLink::midPoint(Chunk *&chunk) const
{
    Vector3 startPt, endPt;
    getEndPoints(startPt, endPt, true);
    Vector3 midPt = 0.5f*(startPt + endPt);
    bool found = true;
    midPt.y = 
        heightAtPos(midPt.x, midY_ + NEXT_HEIGHT_SAMPLE_MID, midPt.z, &found);
    if (!found)
        midPt.y = 0.5f*(startPt.y + endPt.y);
    int wgx = (int)floor(midPt.x/GRID_RESOLUTION);
    int wgy = (int)floor(midPt.z/GRID_RESOLUTION);
    chunk = getChunk(wgx, wgy);
    midPt = chunk->transformInverse().applyPoint(midPt);
    return midPt;
}


/**
 *  This function gets the bounds of the link.
 *
 *  @param bb               This is set to the bounds.
 */
/*virtual*/ void EditorChunkLink::edBounds(BoundingBox &bb) const
{
    bb = bb_;
}


/**
 *  Yes, this is an EditorChunkLink.
 *
 *  @return                 True.
 */
/*virtual*/ bool EditorChunkLink::isEditorChunkLink() const
{
    return true;
}


/**
 *  EditorChunkLink's are not selectable, though you can right-click on them.
 *
 *  @return                 False.
 */
/*virtual*/ bool EditorChunkLink::edCanAddSelection() const 
{ 
    return false; 
}


/**
 *  Handle link visibility.
 *
 *  @return		True if visible, false otherwise.
 */
/*virtual*/ bool EditorChunkLink::edShouldDraw() 
{ 
    return
		EditorChunkSubstance<ChunkLink>::edShouldDraw()
		&&
        !!Options::getOptionInt( "render/misc/drawPatrolGraphs", 1 )
		&&
		!BigBang::instance().drawSelection()
        &&
        !Moo::rc().reflectionScene() 
        && 
        !Moo::rc().mirroredTransform()
        &&
        s_enableDraw_;
}


/**
 *  This checks to topological consistency.
 *
 *  @param failureMsg       A failure message if not consistent.
 *
 *  @return                 True if consistent, false otherwise.
 */
/*virtual*/ bool EditorChunkLink::isValid(std::string &failureMsg) const
{
    if (!ChunkLink::isValid(failureMsg))
        return false;

    return true;
}


/**
 *  This function makes the link dirty, as well as the nodes it is linked to.
 */
/*virtual*/ void EditorChunkLink::makeDirty()
{
    Chunk *c = chunk();
    if (c != NULL)
    {
        c->delStaticItem(this);
        c->addStaticItem(this);
        BigBang::instance().changedChunk(c);
    }
    EditorChunkStationNodePtr start = startNode();
    if (start != NULL)
        start->makeDirty();
    EditorChunkStationNodePtr end = endNode();
    if (end != NULL)
        end->makeDirty();
}


/**
 *  This function enables/disables drawing of chunk links.
 */
/*static*/ void EditorChunkLink::enableDraw(bool enable)
{
    s_enableDraw_ = enable;
}


/**
 *  This function recalculates the EditorChunkLink's mesh.
 *
 *  @param s            The start point.
 *  @param e            The end point.
 */
void EditorChunkLink::recalcMesh(Vector3 const &s, Vector3 const &e)
{
    bool foundHeight = true; ; // is the height found?

    // Get the start and add it to the polyline.
    std::vector<Vector3> polyline;
    polyline.push_back(s);  // Add the start

    // Find the difference between the heights at the start/end and the 
    // ground:
    float sd, ed;    
    sd  = s.y - heightAtPos(s.x, s.y + NEXT_HEIGHT_SAMPLE, s.z, &foundHeight);
    if (!foundHeight)
        sd = 0.0f;
    ed  = e.y - heightAtPos(e.x, e.y + NEXT_HEIGHT_SAMPLE, e.z, &foundHeight);
    if (!foundHeight)
        ed = 0.0f;

    bool  inAir = fabs(sd) > AIR_THRESHOLD || fabs(ed) > AIR_THRESHOLD;

    // Choose a segment length that will not make the second last point in
    // the polyline end on the segment.
    float len = (e - s).length();
    float segLength = std::max(len - 2*MAX_SEG_LENGTH, MAX_SEG_LENGTH);
    while (segLength >= MAX_SEG_LENGTH && segLength > MIN_SEG_LENGTH)
        segLength *= 0.5f;
    bool ok          = true;       
    
    // We interpolate between the x,z coordinates of the start and end points
    // and do the following:
    //  1.  Get the height at the interpolated point (h) and interpolate the
    //      height at that point by adding it to the interpolated height
    //      between the start and end (y).
    //  2.  Find the closest point to the mid point and get it's y
    //      coordinate in case the segment is split (midY_).  This helps give a 
    ///     good point to split the link.
    //  3.  Keep track of the minimum and maximum y-value (minY_, maxY_).  This 
    //      helps with the collision detection.
    float lastY      = s.y;
    float midDist    = std::numeric_limits<float>::max();
	// set minY_/maxY_ to the min/max of the start and end points
    minY_ = std::min(s.y, e.y);
    maxY_ = std::max(s.y, e.y);
    float midX = 0.5f*(s.x + e.x);
    float midY = 0.5f*(s.y + e.y);
    midY_ = midY;
    for (float t = MAX_SEG_LENGTH; t < len; t += segLength)
    {
        
        float x = Math::lerp(t, 0.0f, len, s.x, e.x);
        float y = Math::lerp(t, 0.0f, len, s.y, e.y);
        float z = Math::lerp(t, 0.0f, len, s.z, e.z);
        if (!inAir)
        {
            float h =
                heightAtPos
                (
                    x, 
                    lastY + segLength*NEXT_HEIGHT_SAMPLE, 
                    z, 
                    &foundHeight
                ); 
            // If the height was not found then linearly interpolate between
            // the last good point and the end:
            if (!foundHeight)
                h = Math::lerp(t, t -  segLength, len, lastY, e.y);
            y = Math::lerp(t, 0.0f, len, sd, ed) + h;  
            ok &= foundHeight;
        }
        Vector3 thisPos(x, y, z);
        polyline.push_back(thisPos);
        minY_ = std::min(minY_, y);
        maxY_ = std::max(maxY_, y);        
        lastY = y;
        float thisMidDist = (x - midX)*(x - midX) + (y - midY)*(y - midY);
        if (thisMidDist < midDist)
        {
            midDist = thisMidDist;
            midY_ = lastY;
        }
    }

    polyline.push_back(e);  // Add the end

    lastStart_   = s;       // Update so we don't recalculate on every draw
    lastEnd_     = e;
    recalcTime_  = 0.0f;    // Reset recalculation timer
    needRecalc_  = false;

    // Create the index and vertex buffers:
    meshes_.clear();
    if (polyline.size() - 1 > 0)
    {
        vertexBuffer_ = NULL;
        indexBuffer_ = NULL;

        HRESULT hr = 
            Moo::rc().device()->CreateVertexBuffer
            (
                (polyline.size() - 1)*sizeof(ChunkLinkSegment::VertexType)*ChunkLinkSegment::numberVertices(),
                D3DUSAGE_WRITEONLY,
                ChunkLinkSegment::VertexType::fvf(),
                D3DPOOL_MANAGED,
                &vertexBuffer_,
                NULL
            );
        ASSERT(SUCCEEDED(hr));
        hr =
            Moo::rc().device()->CreateIndexBuffer
            (
                (polyline.size() - 1)*sizeof(uint16)*ChunkLinkSegment::numberIndices(),
                D3DUSAGE_WRITEONLY,
                D3DFMT_INDEX16,
                D3DPOOL_MANAGED,
                &indexBuffer_,
                NULL
            );
        ASSERT(SUCCEEDED(hr));

        // Turn the polyline into a mesh:        
        float dist = 0.0;
        for (size_t i = 0; i < polyline.size() - 1; ++i)
        {
            Vector3 const &sp = polyline[i    ];
            Vector3 const &ep = polyline[i + 1];
            float thisDist = (ep - sp).length();
            ChunkLinkSegmentPtr mesh = 
                new ChunkLinkSegment
                (
                    sp, 
                    ep, 
                    MAX_SEG_LENGTH*dist,
                    MAX_SEG_LENGTH*(dist + thisDist),
                    LINK_THICKNESS,
                    vertexBuffer_,
                    indexBuffer_,
                    (uint16)(i*ChunkLinkSegment::numberVertices()),
                    (uint16)(i*ChunkLinkSegment::numberIndices())
                );
            meshes_.push_back(mesh);
            dist += thisDist;
        }
    }

    removeFromLentChunks();

    // Generate a list of the chunks that the link passes through and add them:    
    int lastx = std::numeric_limits<int>::max();
    int lasty = std::numeric_limits<int>::max();
    for (size_t i = 0; i < polyline.size(); ++i)
    {
        Vector3 const &pt = polyline[i];
        int wgx = (int)(pt.x/GRID_RESOLUTION);
        int wgy = (int)(pt.z/GRID_RESOLUTION);
        if (wgx != lastx || wgy != lasty)
        {
            Chunk *loanChunk = getChunk(wgx, wgy);
            if (loanChunk != NULL && loanChunk->loaded() && !loanChunk->loading())
            {
                if (loanChunk->addLoanItem(this))
                    lentChunks_.push_back(loanChunk);
            }
            else
            {
                needRecalc_ = true;
            }
            lastx = wgx;
            lasty = wgy;
        }
    }

    // Update collision information:
    if (chunk() != NULL)
    {
        ChunkLinkObstacle::instance(*chunk()).delObstacles(this);
        for (size_t i = 0; i < lentChunks_.size(); ++i)
        {
            ChunkLinkObstacle::instance(*lentChunks_[i]).delObstacles(this);
        }
        addAsObstacle();
    }
}


/**
 *  This function gets the end points.
 *
 *  @param s        The start point.
 *  @param e        The end point.
 *  @param absoluteCoords   If true then the points are world coordinates,
 *                  if false then they are wrt the chunk's coordinates.
 *
 *  @return         False if the end points don't yet exist.
 */
bool 
EditorChunkLink::getEndPoints
(
    Vector3         &s, 
    Vector3         &e,
    bool            absoluteCoords
) const
{
    EditorChunkItem *start  = (EditorChunkItem *)startItem().getObject();
    EditorChunkItem *end    = (EditorChunkItem *)endItem  ().getObject();

    // Maybe it's still loading...
    if 
    (
        start == NULL || end == NULL 
        || 
        start->chunk() == NULL || end->chunk() == NULL
    )
    {
        return false;
    }

    Vector3 lStartPt        = start->edTransform().applyToOrigin();
    Vector3 lEndPt          = end  ->edTransform().applyToOrigin();
    
    s   = start->chunk()->transform().applyPoint(lStartPt);
    e   = end  ->chunk()->transform().applyPoint(lEndPt  );

    if (!absoluteCoords)
    {
        Matrix m = chunk()->transform();
        m.invert();
        s = m.applyPoint(s);
        e = m.applyPoint(e);
    }

    return true;
}


/**
 *  This function returns the height at the given position.  
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate to search down from.
 *  @param z        The z coordinate.
 *  @param found    If not NULL this will be set to true/false if something
 *                  was found.
 *  
 *  @return         The height at the given position.
 */
float EditorChunkLink::heightAtPos(float x, float y, float z, bool *found) const
{    
    // Look downwards from an estimated position:
    Vector3 srcPos(x, y, z);
    Vector3 dstPos(x, -MAX_SEARCH_HEIGHT, z);    
    float dist = 
        ChunkManager::instance().cameraSpace()->collide
        (
            srcPos, 
            dstPos, 
            ClosestObstacleNoEditStations()
        );
    float result = 0.0f;
    if (dist > 0.0f)
    {
        result = y - dist;
        if (found != NULL)
            *found = true;
    }
    else
    {
        // That didn't work, look upwards:
        srcPos = Vector3(x, y, z);
        dstPos = Vector3(x, +MAX_SEARCH_HEIGHT, z);  
        dist = 
            ChunkManager::instance().cameraSpace()->collide
            (
                srcPos, 
                dstPos, 
                ClosestObstacleNoEditStations()
            );
        if (dist > 0.0f)
        {
            result = y + dist;
            if (found != NULL)
                *found = true;
        }
        // No height found, give up.  This can occur if, for example, a 
        // chunk's terrain has not yet loaded.
        else
        {
            if (found != NULL)
                *found = false;
        }
    }
    return result;
}


/**
 *  This function gets the chunk at the given position.
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate.
 *
 *  @return         The chunk at the given position.
 */
Chunk *EditorChunkLink::getChunk(int x, int y) const
{
    std::string chunkName;
    int16 wgx = (int16)x;
    int16 wgy = (int16)y;
    ::chunkID(chunkName, wgx, wgy);
    Chunk *chunk = 
        ChunkManager::instance().findChunkByName
        (
            chunkName,
            BigBang::instance().chunkDirMapping(),
            false // don't create chunk if one doesn't exist
        );
    return chunk;
}


/**
 *  This function removes the link from the lent-out chunks.
 */
void EditorChunkLink::removeFromLentChunks()
{
    // Remove from the list of loaned chunks:
    for (size_t i = 0; i < lentChunks_.size(); ++i)
    {
        if (lentChunks_[i]->isLoanItem(this))
            lentChunks_[i]->delLoanItem(this);
    }
    lentChunks_.clear();    
}


/**
 *  This function is useful to cast the start as an EditorChunkStationNodePtr.
 *
 *  @return         The start as an EditorChunkStationNodePtr.
 */
EditorChunkStationNodePtr EditorChunkLink::startNode() const
{
    if (startItem() != NULL && startItem()->isEditorChunkStationNode())
        return static_cast<EditorChunkStationNode *>(startItem().getObject());
    else
        return EditorChunkStationNodePtr();
}


/**
 *  This function is useful to cast the end as an EditorChunkStationNodePtr.
 *
 *  @return         The end as an EditorChunkStationNodePtr.
 */
EditorChunkStationNodePtr EditorChunkLink::endNode() const
{
    if (endItem() != NULL && endItem()->isEditorChunkStationNode())
        return static_cast<EditorChunkStationNode *>(endItem().getObject());
    else
        return EditorChunkStationNodePtr();
}


/**
 *  This function finds neighbourhood links.  These are links that are 
 *  connected to this go through nodes that only have two links.
 */ 
void EditorChunkLink::neighborhoodLinks(std::vector<Neighbor> &neighbors) const
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

    // Handle the degenerate case:
    if (start == NULL || end == NULL)
        return;

    // This link is a neighbour to itself, which may seem a little odd!
    Neighbor myself;
    myself.first  = const_cast<EditorChunkLink *>(this);
    myself.second = true;
    neighbors.push_back(myself);

    // Find all links in the start direction:
    EditorChunkStationNodePtr node         = start;
    EditorChunkLinkPtr        link         = const_cast<EditorChunkLink *>(this);
    bool                      done         = false;
    bool                      sameDirAs1st = true;
    while (node && node->numberLinks() == 2 && !done)
    {        
        done = true;
        for (size_t i = 0; i < node->numberLinks() && done; ++i)
        {
            EditorChunkLinkPtr nextLink = node->getLink(i);
            if (nextLink != NULL && nextLink != this && nextLink != link)
            {       
                EditorChunkStationNodePtr nextNode = NULL;
                // Get the next node:
                if (nextLink->startNode() != node)
                    nextNode = nextLink->startNode();
                else
                    nextNode = nextLink->endNode();
                // Have we added the new link yet?  If we have then we've gone
                // in a cycle and we are done:
                for (size_t k = 0; k < neighbors.size() && done; ++k)
                {
                    if (neighbors[k].first == nextLink.getObject())
                    {
                        done = true;
                    }
                    else
                    {
                        // Are they in opposite directions?
                        bool sameDir = true;
                        if 
                        (
                            nextLink->startNode() == link->startNode()
                            ||
                            nextLink->endNode() == link->endNode()
                        )
                        {
                            sameDir = false;
                        }
                        if (!sameDir)
                            sameDirAs1st = !sameDirAs1st;
                        Neighbor neighbor;
                        neighbor.first  = nextLink.getObject();
                        neighbor.second = sameDirAs1st;
                        // Add to the neighbor list:
                        neighbors.push_back(neighbor);
                        // There is more to do.
                        done = false; 
                    }
                }
                node = nextNode;
                link = nextLink;
            }
        }
    }

    // Find all links in the end direction:
    node         = end;
    link         = const_cast<EditorChunkLink *>(this);
    done         = false;
    sameDirAs1st = true;
    while (node && node->numberLinks() == 2 && !done)
    {        
        done = true;
        for (size_t i = 0; i < node->numberLinks() && done; ++i)
        {
            EditorChunkLinkPtr nextLink = node->getLink(i);
            if (nextLink != NULL && nextLink != this && nextLink != link)
            {       
                EditorChunkStationNodePtr nextNode = NULL;
                // Get the next node:
                if (nextLink->startNode() != node)
                    nextNode = nextLink->startNode();
                else
                    nextNode = nextLink->endNode();
                // Have we added the new link yet?  If we have then we've gone
                // in a cycle and we are done:
                for (size_t k = 0; k < neighbors.size() && done; ++k)
                {
                    if (neighbors[k].first == nextLink.getObject())
                    {
                        done = true;
                    }
                    else
                    {
                        // Are they in opposite directions?
                        bool sameDir = true;
                        if 
                        (
                            nextLink->startNode() == link->startNode()
                            ||
                            nextLink->endNode() == link->endNode()
                        )
                        {
                            sameDir = false;
                        }
                        if (!sameDir)
                            sameDirAs1st = !sameDirAs1st;
                        Neighbor neighbor;
                        neighbor.first  = nextLink.getObject();
                        neighbor.second = sameDirAs1st;
                        // Add to the neighbor list:
                        neighbors.push_back(neighbor);
                        // There is more to do.
                        done = false; 
                    }
                }
                node = nextNode;
                link = nextLink;
            }
        }
    }
}


/**
 *  This is called to delete a link.
 */
void EditorChunkLink::deleteCommand()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

	UndoRedo::instance().add
    (
        new StationLinkOperation
        (
            start, 
            end, 
            start->getLinkDirection(end.getObject())
        ) 
    );
    UndoRedo::instance().barrier("Delete link", false);
    start->removeLink(end->id());
    start->makeDirty();
    end  ->makeDirty();
    std::vector<ChunkItemPtr> emptyList;
    BigBang::instance().setSelection(emptyList, true);
}


/**
 *  This is called to swap the direction of a link.
 */
void EditorChunkLink::swapCommand()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

	UndoRedo::instance().add
    (
        new StationLinkOperation
        (
            start, 
            end, 
            start->getLinkDirection(end.getObject())
        ) 
    );
    UndoRedo::instance().barrier("Swap link direction", false);
    Direction newDir = direction();
    if (direction() == ChunkLink::DIR_START_END)
        newDir = ChunkLink::DIR_END_START;
    else if (direction() == ChunkLink::DIR_END_START)
        newDir = ChunkLink::DIR_START_END;
    else
        newDir = ChunkLink::DIR_START_END;
    start->setLink(end  .getObject(), ((newDir & DIR_START_END) != 0));
    end  ->setLink(start.getObject(), ((newDir & DIR_END_START) != 0));
    makeDirty();
}


/**
 *  This is used to set the direction of a link to both directions.
 */
void EditorChunkLink::bothDirCommand()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

	UndoRedo::instance().add
    (
        new StationLinkOperation
        (
            start, 
            end, 
            start->getLinkDirection(end.getObject())
        ) 
    );
    UndoRedo::instance().barrier("Both link directions", false);
    direction(ChunkLink::DIR_BOTH);
    start->setLink(end  .getObject(), true);
    end  ->setLink(start.getObject(), true);
    makeDirty();
}


/**
 *  This is used to swap the direction of a run of links.
 */
void EditorChunkLink::swapRunCommand()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

    std::vector<Neighbor> links;
    neighborhoodLinks(links);
    Direction newDir, oppositeDir;
    if (direction() == ChunkLink::DIR_START_END)
    {
        newDir      = ChunkLink::DIR_END_START;
        oppositeDir = ChunkLink::DIR_START_END;
    }
    else if (direction() == ChunkLink::DIR_END_START)
    {
        newDir      = ChunkLink::DIR_START_END;
        oppositeDir = ChunkLink::DIR_END_START;
    }
    else
    {
        newDir      = ChunkLink::DIR_START_END;
        oppositeDir = ChunkLink::DIR_END_START;
    }
    for (size_t i = 0; i < links.size(); ++i)
    {
        start = links[i].first->startNode();
        end   = links[i].first->endNode();
        UndoRedo::instance().add
        (
            new StationLinkOperation
            (
                start, 
                end, 
                start->getLinkDirection(end.getObject())
            ) 
        );
        if (links[i].second)
        {
            start->setLink(end  .getObject(), (newDir & DIR_START_END) != 0);
            end  ->setLink(start.getObject(), (newDir & DIR_END_START) != 0);
        }
        else
        {
            start->setLink(end  .getObject(), (oppositeDir & DIR_START_END) != 0);
            end  ->setLink(start.getObject(), (oppositeDir & DIR_END_START) != 0);
        }
        links[i].first->makeDirty();
    }
    UndoRedo::instance().barrier("Swap link direction", false);
}


/**
 *  This is used to set the direction of a run of links to both.
 */
void EditorChunkLink::bothDirRunCommand()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

    std::vector<Neighbor> links;
    neighborhoodLinks(links);
    Direction newDir = direction();
    if (newDir == ChunkLink::DIR_START_END)
        newDir = ChunkLink::DIR_END_START;
    else if (newDir == ChunkLink::DIR_END_START)
        newDir = ChunkLink::DIR_START_END;
    else
        newDir = ChunkLink::DIR_START_END;
    for (size_t i = 0; i < links.size(); ++i)
    {
        start = links[i].first->startNode();
        end   = links[i].first->endNode();
        UndoRedo::instance().add
        (
            new StationLinkOperation
            (
                start, 
                end, 
                start->getLinkDirection(end.getObject())
            ) 
        );
        start->setLink(end  .getObject(), true);
        end  ->setLink(start.getObject(), true);
        links[i].first->makeDirty();
    }
    UndoRedo::instance().barrier("Both link direction", false);
}


/**
 *  This is called to split a link.
 */
void EditorChunkLink::splitCommand()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

    start->split(*end);
    makeDirty();
}


/**
 *  This is used to validate all of the graphs.  It displays an error message
 *  if something is not valid and does nothing if it is.
 */
void EditorChunkLink::validateCommand()
{
    bool ok = true;
    std::string failureMsg;
    std::vector<StationGraph*> graphs = StationGraph::getAllGraphs();
    for (size_t i = 0; i < graphs.size() && ok; ++i)
    {
        if (!graphs[i]->isValid(failureMsg))
            ok = false;
    }
    if (!ok)
    {
        AfxMessageBox(failureMsg.c_str());
    }
}


/**
 *  This is called to delete a link with an entity.
 */
void EditorChunkLink::deleteLinkCommand()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

    // Sort out which is the node and which is the entity:
    EditorChunkStationNodePtr node = start;
    EditorChunkEntity *entity = 
        static_cast<EditorChunkEntity *>(endItem().getObject());
    if (end != NULL)
    {
        node = end;
        entity = static_cast<EditorChunkEntity *>(startItem().getObject());
    }
    // Set an undo point:
	UndoRedo::instance().add
    (
        new StationEntityLinkOperation
        (
            entity
        ) 
    );
    UndoRedo::instance().barrier("Delete link", false);
    // Do the link deletion:
    entity->disconnectFromPatrolList();
    makeDirty();
}


/**
 *  This is called to force the graph to load of the chunks that it's nodes
 *  are in.
 */
void EditorChunkLink::loadGraph()
{
    EditorChunkStationNodePtr start = startNode();
    EditorChunkStationNodePtr end   = endNode();

    if (start != NULL && start->graph() != NULL)
        start->graph()->loadAllChunks(BigBang::instance().chunkDirMapping());
    else if (end != NULL && end->graph() != NULL)
        end->graph()->loadAllChunks(BigBang::instance().chunkDirMapping());
}
