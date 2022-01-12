#include "Python.h"
#include "network/bundle.hpp"
#include <cstdlib>
#include <arpa/inet.h>
#include "cellapp/cellapp.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"

void executeRemoteScript( std::string script, std::string address )
{
	size_t colonPos = address.find_last_of( ':' );
        if ( colonPos == std::string::npos )
	{
		ERROR_MSG("EgExtra::executeGlobalScript: %s has no colon\n", address.c_str() );
                return;
	}

	in_addr ip;
	u_int16_t port;
	if (inet_aton( address.substr( 0, colonPos ).c_str(), &ip ) == -1)
	{
		ERROR_MSG("EgExtra::executeGlobalScript: Could not parse IP address %s", address.substr( 0, colonPos ).c_str() );
		return;
	}
	port = htons( std::atoi( address.substr( colonPos + 1, address.length() - colonPos - 1 ).c_str() ) );

	Mercury::Channel & channel = CellApp::instance().getChannel( Mercury::Address( ip.s_addr, port ) );

	Mercury::Bundle & bundle = channel.bundle();
	bundle.startMessage( CellAppInterface::runScript );
	bundle << script;
	channel.send();
}


PY_AUTO_MODULE_FUNCTION( RETVOID, executeRemoteScript, ARG( std::string, ARG( std::string, END ) ), BigWorld )



// execute_global_script.cpp
