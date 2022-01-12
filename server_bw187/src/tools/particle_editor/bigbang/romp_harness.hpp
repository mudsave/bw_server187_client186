/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

//---------------------------------------------------------------------------

#ifndef romp_test_harnessH
#define romp_test_harnessH

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "resmgr/datasection.hpp"

class EnviroMinder;
class TimeOfDay;

class RompHarness : public PyObjectPlus
{
	Py_Header( RompHarness, PyObjectPlus )
public:
	RompHarness( PyTypePlus * pType = &s_type_ );

    bool	init();

    void	setTime( float t );
	void	setSecondsPerHour( float sph );
	void	fogEnable( bool state );
	bool	fogEnable() const;
    void	setRainAmount( float r );
    void	propensity( const std::string& weatherSystemName, float amount );

    void	update( float dTime, bool globalWeather );
    void	drawPreSceneStuff();
	void	drawDelayedSceneStuff();
    void	drawPostSceneStuff();

	bool	useShimmer() const	{ return useShimmer_; }

    TimeOfDay*	timeOfDay() const;
	EnviroMinder& enviroMinder() { return *enviroMinder_; }

	//-------------------------------------------------
	//Python Interface
	//-------------------------------------------------
	PyObject *	pyGetAttribute( const char * attr );
	int			pySetAttribute( const char * attr, PyObject * value );

	//methods
	PY_METHOD_DECLARE( py_setTime )
	PY_METHOD_DECLARE( py_setSecondsPerHour )
	PY_METHOD_DECLARE( py_setRainAmount )
	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( bool, fogEnable, fogEnable )
    
private:
	float		dTime_;
    EnviroMinder* enviroMinder_;
	class Bloom* bloom_;
	bool		useBloom_;
	class HeatShimmer* shimmer_;
	bool		useShimmer_;
    bool		inited_;
};
//---------------------------------------------------------------------------
#endif
