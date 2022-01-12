AttachMonitor useage:

This baseapp extension will attach a callback to a proxy to receive a message every time the attached client calls a server script message.


For example:

class Account( BigWorld.Proxy ):
	def callback( methodName, methodID, entityID, args ):
		print methodName, methodID, entityID, args

	def monitor( self ):
		BigWorld.attachMonitor( self, callback )


This will log every server method call from this client.

client/Account.py:

class Account( BigWorld.Proxy ):
	def callback( methodName, methodID, entityID, args ):
		print methodName, methodID, entityID, args


base/Account.py:

class Account( BigWorld.Proxy ):
	def attachMonitor( self, mailbox ):
		BigWorld.attachMonitor( self, mailbox.client.callback )

	def monitor( self, mailbox ):
	
The format of the callback is:
def callback( methodName, methodID, entityID, args ):
methodname: a python string that is the name of the method, or "unknown" if it cannot be worked out by the baseapp.
methodID: the numerical ID of the function.
entityID: the ID of the entity receiving the function call.
args: a python tuple with the arguments of the function.


BigWorld.attachMonitor must be called from the Baseapp with the proxy that is being monitored, I recommend that it is called with "self" as the first argument to ensure that. 
The callback can be a remote client method on the Game Master's client, or a base method, or whatever is required.
