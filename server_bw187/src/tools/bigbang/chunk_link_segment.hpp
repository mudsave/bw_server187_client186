/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_LINK_SEGMENT_HPP
#define CHUNK_LINK_SEGMENT_HPP


#include "cstdmf/smartpointer.hpp"
#include "math/vector3.hpp"
#include "chunk/chunk_link.hpp"
#include "moo/vertex_formats.hpp"


class   Moo::RenderContext;


/**
 *  This class represents a segment in an EditorChunkLink.
 */
class ChunkLinkSegment : public ReferenceCount
{
public:
    typedef ComObjectWrap<DX::VertexBuffer>     VertexBuffer;
    typedef ComObjectWrap<DX::IndexBuffer>      IndexBuffer;
    typedef Moo::VertexXYZNUV2                  VertexType;

    ChunkLinkSegment
    (
        Vector3                 const &start,
        Vector3                 const &end,
        float                   startU,
        float                   endU,
        float                   thickness,
        VertexBuffer            vertexBuffer,
        IndexBuffer             indexBuffer,
        uint16                  vertexBase,
        uint16                  indexBase
    );

    static void 
    beginDrawSegments
    (
        Moo::RenderContext      &rc,
        DX::Texture             *texture,
        Moo::EffectMaterial     *effectMaterial,
        float                   time,
        float                   vSpeed,
        ChunkLink::Direction    direction
    );

    static void draw
    (
        Moo::RenderContext      &rc,
        VertexBuffer            vertexBuffer,
        IndexBuffer             indexBuffer,
        unsigned int            numSegments
    );

    void drawTris
    (
        Moo::RenderContext      &rc
    ) const;

    static void 
    endDrawSegments
    (
        Moo::RenderContext      &rc,
        DX::Texture             *texture,
        Moo::EffectMaterial     *effectMaterial,
        float                   time,
        float                   vSpeed,
        ChunkLink::Direction    direction
    );

    bool intersects
    (
        Vector3                 const &start,
        Vector3                 const &dir,
        float                   &t,
        WorldTriangle           &tri
    ) const;

    static unsigned int numberVertices();

    static unsigned int numberIndices();

    static unsigned int numberTriangles();

private:
    void addVertex(VertexType *buffer, VertexType const &v);

    void 
    addTriangle
    (
        VertexType                  const &v1,
        VertexType                  const &v2,
        VertexType                  const &v3
    );

    ChunkLinkSegment(ChunkLinkSegment const &);
    ChunkLinkSegment &operator=(ChunkLinkSegment const &);

private:           
    Matrix                          transform_;
    Matrix                          invTransform_;
    Vector3                         min_;
    Vector3                         max_;
    VertexBuffer                    vertexBuffer_;
    IndexBuffer                     indexBuffer_;
    std::vector<WorldTriangle>      triangles_;
    float                           startU_;
    float                           endU_;
};


typedef SmartPointer<ChunkLinkSegment>  ChunkLinkSegmentPtr;

#endif // CHUNK_LINK_SEGMENT_HPP
