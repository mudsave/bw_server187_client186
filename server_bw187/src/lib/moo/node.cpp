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

#include "resmgr/datasection.hpp"

#ifndef MF_SERVER
#include "render_context.hpp"
#endif
#ifdef _WIN32
#include "math/blend_transform.hpp"
#endif

#include "node.hpp"

#ifndef CODE_INLINE
#include "node.ipp"
#endif

#include "cstdmf/memory_counter.hpp"

memoryCounterDefine( node, Geometry );

namespace Moo
{


/// Static initialiser
int Node::s_blendCookie_ = 1024;

/**
 * Default node constructor
 */
Node::Node() :
	transform_( Matrix::identity ),
	worldTransform_( Matrix::identity ),
	blendCookie_( s_blendCookie_ - 16 ),
	blendRatio_( 0.f ),
#ifdef _WIN32
	blendTransform_( transform_ ),
#endif
	transformInBlended_( false ),
	parent_( NULL )
{
	memoryCounterAdd( node );
	memoryClaim( this );
}

/**
 * Node destructor
 */
Node::~Node()
{
	memoryCounterSub( node );
	memoryClaim( this );
	memoryClaim( children_ );
	memoryClaim( identifier_ );
}


#ifndef MF_SERVER
/**
 * Traverse the hierarchy and update worldtransforms.
 */
void Node::traverse( )
{
	// Update transform
	Moo::rc().push();
	Moo::rc().preMultiply( this->transform() );
	worldTransform_ = Moo::rc().world();

	// Iterate and recurse through children
	NodePtrVector::iterator it = children_.begin();
	NodePtrVector::iterator end = children_.end();
	while (it != end)
	{
		(*it)->traverse();
		++it;
	}

	Moo::rc().pop();
}
#endif // not MF_SERVER

/**
 * Add a child node
 */
void Node::addChild( NodePtr node )
{
	node->removeFromParent();
	children_.push_back( node );
	node->parent_ = this;
}

/**
 * Remove a child node
 */
void Node::removeChild( NodePtr node )
{
	NodePtrVector::iterator it = std::find( children_.begin(), children_.end(), node );
	MF_ASSERT( it != children_.end() );
	if( it != children_.end() )
	{
		(*it)->parent_ = NULL;
		children_.erase( it );
	}
}

/**
 * Remove this node from the parent
 */
void Node::removeFromParent( )
{
	if( parent_ )
	{
		parent_->removeChild( NodePtr( this ) );
	}
}


/**
 * Find a node recursively
 *
 * @param identifier name of node to find
 *
 * @return smartpointer to the node
 */
NodePtr Node::find( const std::string& identifier )
{
	// Is this the node we are looking for?
	if( identifier == identifier_ )
		return this;

	// Iterate through children, do a recursive find
	NodePtrVector::iterator it = children_.begin();
	NodePtrVector::iterator end = children_.end();
	while( it != end )
	{
		NodePtr ret = (*it)->find( identifier );
		if( ret )
			return ret;
		it++;
	}

	// The node is not in this branch.
	return NULL;
}


/**
 * Count the number of descendants this node has
 *
 * @return the number of descendants
 */
uint32 Node::countDescendants( ) const
{
	uint32 descendants = children_.size();
	NodePtrVector::const_iterator it = children_.begin();
	NodePtrVector::const_iterator end = children_.end();
	while( it != end )
	{
		descendants += (*it)->countDescendants();
		++it;
	}
	return descendants;
}

/**
 * Load nodes recursively
 *
 * @param nodeSection a DataSectionPtr to the node data to load
 *
 */
void Node::loadRecursive( DataSectionPtr nodeSection )
{
	memoryCounterAdd( node );

	identifier_ = nodeSection->readString( "identifier" );
	memoryClaim( identifier_ );

	transform_ = nodeSection->readMatrix34( "transform", Matrix::identity );
	transformInBlended_ = false;

	std::vector< DataSectionPtr > nodeSections;
	nodeSection->openSections( "node", nodeSections );
	std::vector< DataSectionPtr >::iterator it = nodeSections.begin();
	std::vector< DataSectionPtr >::iterator end = nodeSections.end();

	while( it != end )
	{
		NodePtr childNode = new Node;
		childNode->loadRecursive( *it );
		this->addChild( childNode );
		++it;
	}
	memoryClaim( children_ );
}

/**
 * Set this nodes identifier
 *
 * @param identifier the nodes identifier
 */
void Node::identifier( const std::string& identifier )
{
	identifier_ = identifier;

	memoryCounterAdd( node );
	memoryClaim( identifier_ );
}

}

// node.cpp
