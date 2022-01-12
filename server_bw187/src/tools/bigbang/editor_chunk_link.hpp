/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_LINK_HPP
#define EDITOR_CHUNK_LINK_HPP


#include "chunk/chunk_link.hpp"
#include "chunks/editor_chunk.hpp"
#include "editor_chunk_substance.hpp"
#include "editor_chunk_station.hpp"
#include "chunk_link_segment.hpp"
#include "appmgr/options.hpp"
#include "moo/moo_math.hpp"
#include "moo/base_texture.hpp"
#include "big_bang.hpp"


/**
 *  This class represents a link between items.  These links need to be created
 *  by the objects that have links, they are not saved.
 */
class EditorChunkLink : public EditorChunkSubstance<ChunkLink>, public Aligned
{
    DECLARE_EDITOR_CHUNK_ITEM(EditorChunkLink)

public:
    EditorChunkLink();
    ~EditorChunkLink();

	virtual bool edAffectShadow() const {	return false; }

    /*virtual*/ void draw();

    /*virtual*/ char const *sectName() const;

    /*virtual*/ const char * drawFlag() const;

    /*virtual*/ ModelPtr reprModel() const;

    /*virtual*/ const Matrix & edTransform();

    /*virtual*/ void toss(Chunk *chunk);

    /*virtual*/ void tick(float dtime);

    /*virtual*/ void addAsObstacle();

    /*virtual*/ std::vector<std::string> 
    edCommand
    (
        std::string     const&path
    ) const;

    /*virtual*/ bool 
    edExecuteCommand
    ( 
        std::string     const &path, 
        std::vector<std::string>::size_type index 
    );

    float
    collide
    (
        Vector3         const &source, 
        Vector3         const &dir,
        WorldTriangle   &wt
    ) const;

    Vector3 midPoint(Chunk *&chunk) const;

    /*virtual*/ void edBounds(BoundingBox &bb) const;

    /*virtual*/ bool isEditorChunkLink() const;

	/*virtual*/ bool edIsSnappable() { return false; }

    /*virtual*/ bool edCanAddSelection() const;

	/*virtual*/ bool edShouldDraw();

    /*virtual*/ bool isValid(std::string &failureMsg) const;

    /*virtual*/ void makeDirty();

    static void enableDraw(bool enable);

protected:
    // Neighbor links to this one are links before and after this link, as
    // well as this link that go through nodes that only have a two links.
    // The second parameter is true if the link goes is in the same order
    // as the original link (e.g. this->start == link->end) or false if in the 
    // opposite order (e.g. this->start == link->start.)
    typedef std::pair<EditorChunkLink *, bool> Neighbor;

    void recalcMesh(Vector3 const &s, Vector3 const &e);

    bool getEndPoints(Vector3 &s, Vector3 &e, bool absoluteCoords) const;

    float heightAtPos(float x, float y, float z, bool *foundHeight = NULL) const;

    Chunk *getChunk(int x, int y) const;

    void removeFromLentChunks();

    EditorChunkStationNodePtr startNode() const;

    EditorChunkStationNodePtr endNode() const;    

    void neighborhoodLinks(std::vector<Neighbor> &neighbors) const;

    void deleteCommand();

    void swapCommand();

    void bothDirCommand();

    void swapRunCommand();

    void bothDirRunCommand();

    void splitCommand();

    void validateCommand();

    void deleteLinkCommand();

    void loadGraph();

private:
    EditorChunkLink(EditorChunkLink const &);
    EditorChunkLink &operator=(EditorChunkLink const &);

private:
    typedef ComObjectWrap<DX::VertexBuffer>     VertexBuffer;
    typedef ComObjectWrap<DX::IndexBuffer>      IndexBuffer;

    float                               yOffset_;       // offset above ground
    Vector3                             lastStart_;     // last render start point
    Vector3                             lastEnd_;       // last render end point
    std::vector<ChunkLinkSegmentPtr>    meshes_;        // mesh used to draw
    mutable Matrix                      transform_;     // world to local transformation
    float                               totalTime_;     // total time (used to draw moving segment)
    DX::Texture                         *linkTexture_;  // texture used to directional links draw with
    DX::Texture                         *entityTexture_;// texture used to draw entity-node links
    Moo::EffectMaterial                 *meshEffect_;   // effect file used to draw mesh
    std::vector<ChunkPtr>               lentChunks_;    // chunks lent out to
	BoundingBox							bb_;            // bounding box in chunk
    float                               minY_;          // minimum y coord (helps when going over mountains)
    float                               maxY_;          // maximum y coord (helps when going over mountains)
    bool                                needRecalc_;    // is a recalc needed due to a non-loaded chunk?
    double                              recalcTime_;    // time for next recalculation
    float                               midY_;          // height at around the mid point.
    VertexBuffer                        vertexBuffer_;  // vertex buffer for mesh
    IndexBuffer                         indexBuffer_;   // index buffer for mesh

    static bool                         s_enableDraw_;  // drawing enabled?
};


typedef SmartPointer<EditorChunkLink>   EditorChunkLinkPtr;


#endif // EDITOR_CHUNK_LINK_HPP
