/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SCRIPTED_BEHAVIOUR_HPP
#define SCRIPTED_BEHAVIOUR_HPP

#include <iostream>
#include <Python.h>

#include "resmgr/datasection.hpp"

#include "behaviour.hpp"

/**
 *	This class ...
 *	@todo Document this class.
 */
class ScriptedBehaviour : public Behaviour
{
public:
	ScriptedBehaviour();
	~ScriptedBehaviour();

	bool init( DataSectionPtr pSection );

	virtual void onActivate( Player & player, Match & match );
	virtual float priority( const Player & player, const Match & match ) const;
	virtual void apply( float dTime, Player & player, Match & match );

private:
	ScriptedBehaviour( const ScriptedBehaviour& );
	ScriptedBehaviour& operator=( const ScriptedBehaviour& );

	friend std::ostream& operator<<( std::ostream&, const ScriptedBehaviour& );

	PyObject * pScriptObject_;
};

#ifdef CODE_INLINE
#include "scripted_behaviour.ipp"
#endif

#endif // SCRIPTED_BEHAVIOUR_HPP
