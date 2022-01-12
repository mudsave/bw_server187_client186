/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MAIN_APP_HPP
#define MAIN_APP_HPP

#include "Python.h"

#include "network/interfaces.hpp"
#include "network/nub.hpp"
#include "network/public_key_cipher.hpp"
#include "pyscript/script.hpp"

class ClientApp;
class MovementController;
class MovementFactory;
class PythonServer;

class MainApp : public Mercury::TimerExpiryHandler
{
public:
	MainApp();
	virtual		~MainApp();

	bool init( int argc, char * argv[] );
	void run();
	void shutDown();

	void addBot();
	void addBots( int num );
	void delBots( int num );

	void delTaggedEntities( std::string tag );

	void updateMovement( std::string tag );
	void runPython( std::string tag );

	MovementController * createDefaultMovementController( float & speed,
		Vector3 & position );
	MovementController * createMovementController( float & speed,
		Vector3 & position, const std::string & controllerType,
		const std::string & controllerData );

	static void addFactory( const std::string & name,
									MovementFactory & factory );

	virtual int handleTimeout( int, void * );

	// ---- Accessors ----
	const std::string & serverName() const 		{ return serverName_; }
	const std::string & username() const		{ return username_; }
	const std::string & password() const		{ return password_; }
	const std::string & publicKeyPath() const	{ return publicKeyPath_; }
	const std::string & tag() const				{ return tag_; }
	const bool randomName() const				{ return randomName_; }
	const bool hasScripts() const				{ return hasScripts_; }
	const std::string & controllerType() const	{ return controllerType_; }
	const std::string & controllerData() const	{ return controllerData_; }

	void serverName( const std::string & name )	{ serverName_ = name; }
	void username( const std::string & name )	{ username_ = name; }
	void password( const std::string & pswd )	{ password_ = pswd; }
	void tag( const std::string & tag )			{ tag_ = tag; }
	void controllerType( const std::string & v ){ controllerType_ = v; }
	void controllerData( const std::string & v ){ controllerData_ = v; }

	const double & localTime() const			{ return localTime_; }

	Mercury::Nub & nub()						{ return nub_; }

	// ---- Script related Methods ----
	ClientApp * findApp( ObjectID id ) const;
	void appsKeys( PyObject * pList ) const;
	void appsValues( PyObject * pList ) const;
	void appsItems( PyObject * pList ) const;

	// ---- Get personality module ----
	PyObjectPtr getPersonalityModule() const;

	// ---- Static Methods ----
	static MainApp & instance()					{ return *pInstance_; }
	static MainApp * pInstance()				{ return pInstance_; }

private:
	Mercury::Nub nub_;
	bool stop_;

	std::string serverName_;
	std::string username_;
	std::string password_;
	std::string publicKeyPath_;
	std::string tag_;
	bool randomName_;
	int port_;
	bool hasScripts_;

	typedef std::list< SmartPointer< ClientApp > > Bots;
	Bots bots_;
	double localTime_;
	int timerID_;

	std::string controllerType_;
	std::string controllerData_;

	PythonServer *	pPythonServer_;

	Bots::iterator clientTickIndex_;

	static MainApp * pInstance_;
};

#endif // MAIN_APP_HPP
