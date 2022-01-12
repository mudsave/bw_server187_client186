/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ANIMATION_MANAGER_HPP
#define ANIMATION_MANAGER_HPP

#include <map>
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/concurrency.hpp"

namespace Moo
{

class Animation;
typedef SmartPointer<Animation> AnimationPtr;
class Node;
typedef SmartPointer<Node> NodePtr;

class AnimationManager
{
public:
	~AnimationManager();
	
	static AnimationManager&	instance();
	AnimationPtr				get( const std::string& resourceID, NodePtr rootNode );
	AnimationPtr				get( const std::string& resourceID,
		StringHashMap<NodePtr> & catalogue, SimpleMutex & catalogueLock );

	/*
	 * Public so that ModelViewer can change the original Animation when
	 * fiddling with compression.
	 */
	AnimationPtr				find( const std::string& resourceID );

	std::string					resourceID( Animation* pAnim );

	void fullHouse( bool noMoreEntries = true );

	typedef std::map< std::string, Animation * > AnimationMap;

private:
	AnimationMap				animations_;
	SimpleMutex					animationsLock_;

	bool						fullHouse_;

	void						del( Animation * pAnimation );
	friend class Animation;

    static AnimationManager *   instance_;

	AnimationManager();
	AnimationManager(const AnimationManager&);
	AnimationManager& operator=(const AnimationManager&);
};

}

#ifdef CODE_INLINE
#include "animation_manager.ipp"
#endif


#endif // ANIMATION_MANAGER_HPP
