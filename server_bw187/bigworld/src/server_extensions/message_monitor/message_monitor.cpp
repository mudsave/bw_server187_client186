#include "Python.h"
#include "network/message_filter.hpp"
#include "entitydef/entity_description.hpp"
#include "baseapp/proxy.hpp"
#include "baseapp/baseapp.hpp"

class ForwardingMessageFilter : public Mercury::MessageFilter
{
public:
	ForwardingMessageFilter( PyObjectPtr pCallBack ) :
		pCallBack_( pCallBack )
	{
	}
	void filterMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		Mercury::InputMessageHandler * pHandler )
	{
		if (header.identifier < 0x80)
		{
			pHandler->handleMessage( srcAddr, header, data );
			return;
		}

		MemoryOStream executeCopy( data.remainingLength() );
		executeCopy.transfer( data, data.remainingLength() );
		MemoryIStream forwardCopy( executeCopy.data(), 
			executeCopy.size() );

		ProxyPtr pProxy =
			BaseApp::instance().getProxyForCall();
		if (!pProxy)
			return;
		if (header.identifier >= 0xC0)
		{
			this->parseScriptMessage( forwardCopy, 
				header.identifier & 0x3F, pProxy->id(),
				pProxy->pType()->description().base() );
		}
		else
		{
			EntityID destObject;
			forwardCopy >> destObject;
			if (destObject == 0)
			{
				destObject = pProxy->id();
			}
			BaseApp::Bases::const_iterator iter = 
				BaseApp::instance().bases().find( destObject );
			if (iter != BaseApp::instance().bases().end())
			{
				this->parseScriptMessage( forwardCopy,
					header.identifier & 0x3F, destObject,
					iter->second->pType()->description().cell() );
			}
			else
			{
				this->parseUnknownMessage(
					header.identifier & 0x3F, destObject );
			}
			// If this entity is not local, we do not know its
			// type so we cannot decode it
		}
		
		if (pHandler)
			pHandler->handleMessage( srcAddr, header, executeCopy );
	}

	void parseScriptMessage( BinaryIStream & data, int type, EntityID eid,
				 const EntityDescription::Methods & methods )
	{
		MethodDescription * pMethodDescription = 
			methods.exposedMethod( type, data );
		if (!pMethodDescription)
		{
			WARNING_MSG( "ForwardingMessageFilter::parseScriptMessage could not find method %i\n", type );
			return;
		}

		PyObjectPtr pArgs = pMethodDescription->getArgsAsTuple( data );
		if (pArgs.get() == NULL)
		{
			WARNING_MSG( "ForwardingMessageFilter::parseScriptMessage: Could not destream arguments\n" );
			return;
		}

		PyObjectPtr returnval(
			PyObject_CallFunction( pCallBack_.get(), "siiO", 
				pMethodDescription->name().c_str(), type, eid, 
				pArgs.get() ), true );
		PyErr_Clear();
	}

	void parseUnknownMessage( int type, EntityID eid )
	{
		PyObjectPtr returnval( PyObject_CallFunction( pCallBack_.get(), 
			"sii()", "unknown", type, eid ), true );
		PyErr_Clear();
	}

private:
	PyObjectPtr pCallBack_;
};


bool attachMonitor( PyObjectPtr pProxyObject, PyObjectPtr pCallback )
{
	if (!Proxy::Check( pProxyObject.get() ))
	{
		PyErr_SetString( PyExc_ValueError, "Argument must be a proxy" );
		return false;
	}

	if (!PyCallable_Check( pCallback.get() ))
	{
		PyErr_SetString( PyExc_ValueError, "Argument must be a proxy" );
		return false;
	}

	Proxy * pProxy = static_cast< Proxy * >( pProxyObject.get() );
	pProxy->clientChannel().pMessageFilter( new ForwardingMessageFilter( pCallback ) );
	return true;
}


PY_AUTO_MODULE_FUNCTION( RETOK, attachMonitor, ARG( PyObjectPtr, ARG( PyObjectPtr, END ) ), BigWorld )



// message_monitor.cpp
