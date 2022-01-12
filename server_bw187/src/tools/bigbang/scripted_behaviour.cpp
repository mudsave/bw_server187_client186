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
#include "scripted_behaviour.hpp"

#include "behaviour_factory.hpp"

#ifndef CODE_INLINE
#include "scripted_behaviour.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Behaviour", 0 )


// -----------------------------------------------------------------------------
// Section: Creator
// -----------------------------------------------------------------------------

/**
 *	This class is the creator for ScriptedBehaviour.
 */
class ScriptedBehaviourCreator : public StandardCreator<Behaviour>
{
public:
	/**
	 *	Constructor.
	 */
	ScriptedBehaviourCreator() : StandardCreator<Behaviour>(
		"ScriptedBehaviour", BehaviourFactory::instance() )
	{
	}

private:
	// Override from BehaviourCreator.
	virtual Behaviour * create( DataSectionPtr pSection )
	{
		return this->createHelper( pSection, new ScriptedBehaviour() );
	}
};

/// The creator for ScriptedBehaviour.
static ScriptedBehaviourCreator s_scriptedBehaviourCreator;


// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ScriptedBehaviour::ScriptedBehaviour() :
	pScriptObject_( NULL )
{
}


/**
 *	Destructor.
 */
ScriptedBehaviour::~ScriptedBehaviour()
{
	Py_XDECREF( pScriptObject_ );
	pScriptObject_ = NULL;
}


// -----------------------------------------------------------------------------
// Section: Behaviour overrides
// -----------------------------------------------------------------------------

/**
 *	This method initialises this behaviour.
 */
bool ScriptedBehaviour::init( DataSectionPtr pSection )
{
	if (!pSection)
	{
		return true;
	}

	DataSectionPtr pScript = pSection->openSection( "script" );

	if (pScript)
	{
		PyObject * pModule = PyImport_ImportModule( const_cast<char *>(
				pScript->readString( "module", "Behaviours" ).c_str() ) );

		if (pModule != NULL)
		{
			Py_XDECREF( pScriptObject_ );
			char * className =
				const_cast<char *>( pScript->readString( "class" ).c_str() );

			pScriptObject_ = PyObject_CallMethod( pModule, className, "" );

			if (pScriptObject_ == NULL)
			{
				ERROR_MSG( "No script object %s\n", className );
				PyErr_Print();
			}
			Py_DECREF( pModule );
		}
		else
		{
			ERROR_MSG( 
					"Could not get module %s\n",
					pScript->readString( "module", "Behaviours" ).c_str() );
			PyErr_Print();
		}
	}
	else
	{
		WARNING_MSG( "Could not find script\n" );
	}

	return true;
}


/**
 *	This method returns the priority of this behaviour.
 */
float ScriptedBehaviour::priority( const Player & player,
	const Match & match ) const
{
	float priority = 0.f;

	if (pScriptObject_ != NULL)
	{
		PyObject * pResult = PyObject_CallMethod( pScriptObject_,
			"priority", "OO", &player, &match );

		if (pResult != NULL)
		{
			if (PyFloat_Check( pResult ))
			{
				priority = PyFloat_AsDouble( pResult );
			}
			else
			{
				// #### This is a leak
				PyObject * str = PyObject_Str( pScriptObject_ );
				ERROR_MSG( "ScriptedBehaviour::priority: "
					"Method must return a float %s.\n",
					PyString_AsString( str ));
				Py_XDECREF( str );
			}

			Py_DECREF( pResult );
		}
		else
		{
			ERROR_MSG( "ScriptedBehaviour::priority: Script call failed!\n" );
			PyErr_Print();
		}
	}

	return priority;
}


/**
 *	This method applies this behaviour.
 */
void ScriptedBehaviour::apply( float dTime, Player & player, Match & match )
{
	if (pScriptObject_ != NULL)
	{
		PyObject * pResult = PyObject_CallMethod( pScriptObject_,
			"apply", "fOO", dTime, &player, &match );

		if (pResult != NULL)
		{
			Py_DECREF( pResult );
		}
		else
		{
			ERROR_MSG( "ScriptBehaviour::apply: Call to apply failed!\n" );
			PyErr_Print();
		}
	}
}


/**
 *	This method is called when this behaviour becomes the active behaviour for a
 *	player. Derived classes should override this method.
 */
void ScriptedBehaviour::onActivate( Player & player, Match & match )
{
	if (pScriptObject_ != NULL)
	{
		PyObject * pResult = PyObject_CallMethod( pScriptObject_,
			"onActivate", "OO", &player, &match );

		if (pResult != NULL)
		{
			Py_DECREF( pResult );
		}
		else
		{
//			ERROR_MSG( "ScriptBehaviour::onActivate: "
//				"Call to onActivate failed!\n" );
//			PyErr_Print();
			PyErr_Clear();
		}
	}
}

// -----------------------------------------------------------------------------
// Section: Streaming
// -----------------------------------------------------------------------------

/**
 *	Streaming operator for ScriptedBehaviour.
 */
std::ostream& operator<<(std::ostream& o, const ScriptedBehaviour& t)
{
	o << "ScriptedBehaviour\n";
	return o;
}

// scripted_behaviour.cpp
