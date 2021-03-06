"""
This module and it's one-size-fits-all 'get' provide an API of sorts for
exposing capabilities for various python objects.  In particular, this is useful
for classes in cluster.py, but since we don't want to pollute that module with
web specific stuff, it lives here.  Hurrah.
"""

import bwsetup; bwsetup.addPath( "../.." )
from pycommon import cluster
from web_console.common import util
import signal

def get( o ):

	options = util.ActionMenuOptions()
	labels = set()

	# Macro to add redirects
	def addRedirect( label, *args, **kw ):
		options.addRedirect( label, *args, **kw )
		labels.add( label )

	# Macro to add script calls
	def addScript( label, *args, **kw ):
		options.addScript( label, *args, **kw )
		labels.add( label )

	# Processes that can be manually stopped
	if isinstance( o, cluster.StoppableProcess ):
		addRedirect( "Stop", "/cc/killproc",
					 params = dict( machine = o.machine.name, pid = o.pid ),
					 help = "Terminate this process" )

		addRedirect( "Hard Kill", "/cc/killproc",
					 params = dict( machine = o.machine.name, pid = o.pid,
									signal = signal.SIGKILL ),
					 help = "Forcefully terminate this process (SIGKILL)" )

		addRedirect( "Quit (Dump Core)", "/cc/killproc",
					 params = dict( machine = o.machine.name, pid = o.pid,
									signal = signal.SIGQUIT ),
					 help = "Terminate this process and dump core (SIGQUIT)" )


	# Processes that have logs
	if isinstance( o, cluster.Process ) and \
	   (o.isServerProc() or isinstance( o, cluster.BotProcess )):
		addScript( "View Log", "Caps.gotoProcessLog",
				   args = (o.user().name, o.componentName(),
						   o.machine.name, o.pid),
				   help = "View the log for this process" )


	# Processes that have python servers
	if isinstance( o, cluster.CellAppProcess ) or \
	   isinstance( o, cluster.BaseAppProcess ) or \
	   isinstance( o, cluster.BotProcess ):

		port = o.getWatcherValue( "pythonServerPort" )
		label = "%s on %s" % (o.label(), o.machine.name)

		if port:
			addRedirect( "Console", "/console/console",
						 params = dict( host = o.machine.ip,
										port = port, process = label ),
						 help = "Connect to the Python console on this process" )

	# Add in any Exposed methods whose names haven't been used already
	if isinstance( o, cluster.Exposed ):
		for func, label, args in o.getExposedMethods():
			if label not in labels:
				addScript(
					label, "Ajax.call",
					args = ("/callExposed",
							dict( method = func,
								  onSuccess = "%s() called successfully" % func,
								  **o.getExposedID() ),
							"", "", args) )

	return options
