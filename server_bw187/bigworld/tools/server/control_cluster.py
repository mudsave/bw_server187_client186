#!/usr/bin/env python

import sys
import os
import re
import optparse
import popen2
import socket
import struct
import operator
import telnetlib
import time
import fnmatch
from StringIO import StringIO

from pycommon import cluster
from pycommon import util
from pycommon import uid as uidmodule
from pycommon import log
from pycommon import messages

USAGE = """%prog [options] [command] [command-options] [command-args]

  If [command] is not given, it defaults to 'display'.

  Most commands run with respect to a particular user, which is you by default,
  or can be overriden with the -u option.

  Like `cvs`, you must specify global options before [command], and options that
  are specific to [command] after it.

  Machine Selection
  =================

  Commands that involve machine selection support any of the following methods:

  * Exact machine name, e.g. 'bw01'
  * Exact IP address, e.g. '10.40.7.1'
  * Shell wildcard match on hostname, e.g. 'bw*'
  * BWMachined version, e.g. 'version:42'
  * Machined group, e.g. 'group:blue'
  * All machines can be explicitly selected with 'all'

  Additionally, the pattern ':noprocs:' matches any machine that has no
  processes registered with bwmachined (useful for updating bwmachined itself).

  In general, if the [machines] argument to a command is optional, not
  specifying any machines selects *all* machines.

  Machined groups are listed in the [Groups] tag in /etc/bwmachined.conf.

  Process Selection
  =================

  Commands that involve process selection support any of the following methods:

  * Exact process name, e.g. 'cellapp01', 'cellappmgr'
  * All processes of a type, e.g. 'cellapps', 'baseapps', 'bots'
  * Single process of a type, *as long as only one is running*, e.g. 'cellapp'
  * Non-zero-padded numbering, e.g. 'cellapp1'
  * Machine:PID (needed only for bots processes), e.g. 'iota:1287'
  * All processes (useful for looking at watchers across the board): 'all'

  The advantage of the fourth pattern is that it allows shell expansions such as
  'cellapp{1..5}' or 'baseapp{1,5,7}'.  Please see the 'Brace Expansion' section
  of the Advanced Bash Scripting Guide for more info.

  Commands
  ========

  start [machines]
    Start the server on the specified set of machines.  To start the server on
    all machines, you must explicitly pass 'all' for machine selection.

    -b,--bots:      Allocate machines for and start bots processes
    -r,--revivers:  Start revivers on each machine used
    -t,--tags:      Restrict what processes can be started on each machine by
                    its [Components] tag list.

  stop
    Shut down the server in the most controlled way possible.  You should always
    use this unless you are sure the server is in a bad state and requires more
    drastic shutdown (see `kill` and `nuke` below)

  kill
    Do a forced shutdown of the server using SIGINT.

  nuke
    Do a forced shutdown of the server using SIGQUIT.

  restart
    Restart the server.  Accepts the same args as 'start', plus:

    -p,--preserve:  Ignore other args and restart server with current layout.
                    If any essential server processes are missing from the
                    current layout, one instance of each will be automatically
                    included.

  startproc <procname> <machine>
    Start the given process on the given machine.

  killproc <processes>
    Kill the specified processes.

  restartproc <process>
    Kill and then restart a process on the same machine.

  display [processes]
    Show detailed information about the current user's server.  If processes are
    manually specified, information is shown about those processes only.

  summary
    Show summarised information about the current user's server.

  check
    Indicate whether a server is running (on stdout and via exit status)

  cinfo <machines>
    Show information about the specified machines running machined in the
    cluster, or all machines running machined if none specified.
    With -v displays the details of the processes running on each machine.

  netinfo <machines>
    Show network statistics for a set of machines.

  users
    Show information about active users on the cluster.  With -v displays the
    details of the processes the user is running.

  save <filename>
    Save the current user's server layout to the given filename.

  load <filename>
    Start a server according to the layout given in the file.

  watch <processes> [watcher-path]
    Query the given watcher path from all processes on the cluster of the
    specified type, or from the process with the given name.  If no watcher path
    is specified, the root directory will be listed.

    'get' is also an alias for this command.

  set <processes> <watcher-path> <new-value>
    Set a watcher to a new value for the specified processes.

  pyconsole [process] [--entity <cell|base>:<id>]
    Telnet to the python server port on the given type of server process with
    the given ID.  Can also connect to the app that a given entity is on with
    --entity.

    --lock-cells:   Prevent cells offloading entities whilst script is executing

  runscript <processes> [script.py]
    Non-interactively run Python script on the given processes.  If script
    filename is not provided on the commandline, script is read from stdin.

    --lock-cells:   Prevent cells offloading entities whilst script is executing

  querytags [tag] [machines]
    Query <tag>.  Passing no arguments is the same as passing --all.  If
    machines are specified but no tag is given, either --all or --list must be
    passed.

    -a,--all:       Show all tag categories and values on selected machines
    -l,--list:      Show all tag categories on selected machines

  mdlist [machines] [options]
    Display a comma separated list of hosts running machined on the local
    network, useful for passing to pdsh's -w option.

    -i|--ip:        Return a list of IP addresses, instead of hostnames
                    (useful when DNS mapping is incomplete)
    -d|--delim <D>: Set delimiter in output

  flush [machines]
    Force machines to flush their tags and user mappings.

  checkring
    Discover the buddy ring on the network and verify its correctness.

  log <message> [machines]
    Send a once-off message to message_logger(s) on the network.  The severity
    of the log message can be passed as any of the standard severity levels
    (TRACE, DEBUG, INFO, etc) and defaults to INFO.

    -s,--severity:  Set log message severity level (default: INFO)

  pyprofile <procs> [-t|--time <secs>] [--cleanup] [--limit <n>] [-o <summary>]
            [--callers] [--callees] [--login-as-server-user]
            [--port <ssh port>]
    Activate Python profiling on the specified set of machines for a short
    amount of time.  Profiles are written to /tmp/<user>-<appname>-<time>.prof
    on each machine, and are scp'ed back to the local machine when done.

    Once all profiles are collected, a readable summary is written to
    /tmp/prof-summary-<time>.  This can be overriden with the -o switch.

    If --cleanup is specified, raw profile data will be deleted automatically
    after the profile summary is displayed.  The summary itself is never
    deleted.

    Number of profiles to show can be limited with the --limit switch.

    Tables of caller and callee functions can be enabled with the --callers and
    --callees switches.

    By default, scp commands are executed as the logged in user, as opposed to
    the user specified with control_cluster.py's -u switch.  If you want to
    execute scp commands as the server user, pass --login-as-server-user.

  cprofile <procs> [-s|--sort <sortkey>] [-l <limit>] [-t|--time <secs>]
    Fetch all C++ profiles from specified processes and display them in a nice
    readable format.  Supported sorts are 'cumTime', 'cumTimePerCall', and
    'count'.  The default is 'cumTime'.

  eventprofile [-t|--time <secs>] [-s|--sort <sortkey>]
	Fetch the count and sizes of events (script calls, property value changes)
	sent to the client over a period of time.
	There are different event counter types:

	publicClientEvents
		Counts and sizes of individual public methods and property changes that
		will be pushed to all/other clients within the entity's AoI.

	privateClientEvents
		Private methods and property changes pushed to an entity's own client.
		Includes bandwidth to clients in bytes per second.

	totalPublicClientEvents
		Total counts and sizes of all public methods and property changes that
		were actually pushed to all/other clients in each entity's AoI.
		Includes bandwidth to clients in bytes per second.

	Supported sorts: 'count', 'totalsize', 'avgsize', 'bandwidth'(default).

""".rstrip()

# For any command that accepts extra switches, specify them here in the format
# that they are passed to OptionParser.add_option()
COMMAND_OPTIONS = { \
	"start":
	[ ( ["-b", "--bots"], {"action": "store_true"} ),
	  ( ["-r", "--revivers"], {"action":"store_true"} ),
	  ( ["-t", "--tags"], {"action":"store_true", "default": True} ) ],

	"restart":
	[ ( ["-p", "--preserve"], {"dest":"preserve", "action":"store_true"} ) ],

	"querytags":
	[ ( ["-a", "--all"], {"dest":"all", "action":"store_true"} ),
	  ( ["-l", "--list"], {"dest":"list", "action":"store_true"} ) ],

	"mdlist":
	[ ( ["-i", "--ip"], {"action":"store_true"} ),
	  ( ["-d", "--delim"], {"default":","} ) ],

	"log":
	[ ( ["-s", "--severity"], {"default":"INFO"} ) ],

	"pyconsole":
	[ ( ["-e", "--entity"], {} ),
	  ( ["--lock-cells"], {"action":"store_true"} ) ],

	"runscript":
	[ ( ["--lock-cells"], {"action":"store_true"} ) ],

	"pyprofile":
	[ ( ["-t", "--time"], {"type": "float", "default": 10.0} ),
	  ( ["-l", "--limit"], {"type": "int", "default": 10} ),
	  ( ["-c", "--cleanup"], {"action": "store_true"} ),
	  ( ["--callers"], {"action": "store_true"} ),
	  ( ["--callees"], {"action": "store_true"} ),
	  ( ["--login-as-server-user"], {"action": "store_true"} ),
	  ( ["-o", "--output"], {} ),
	  ( ["--port"], {'default':22, 'type':'int'} ) ],

	"cprofile":
	[ ( ["-t", "--time"], {"type": "float", "default": 10.0} ),
	  ( ["-s", "--sort"], {"default": "cumTime"} ),
	  ( ["-l", "--limit"], {"type": "int", "default": 20} ) ],

	"eventprofile":
	[ ( ["-t", "--time"], {"type": "float", "default": 10.0} ),
	  ( ["-s", "--sort"], {"default": "bandwidth"} ) ]
	}

# Options for `restart` are same as for `start` plus --preserve
COMMAND_OPTIONS[ "restart" ] = \
				 COMMAND_OPTIONS[ "start" ][:] + \
				 [ ( ["-p","--preserve"],
					 {"dest":"preserve", "action":"store_true"} ) ]

def main():

	# Parse cmdline args
	opt = optparse.OptionParser( USAGE )
	opt.allow_interspersed_args = False
	opt.add_option( "-u", dest = "uid", default = None,
					help = "Specify the UID or username to work with" )
	opt.add_option( "-v", "--verbose", dest = "verbose", action = "store_true",
					help = "Enable verbose debugging output" )
	options, args = opt.parse_args()

	# Enable spew if desired
	if options.verbose:
		cluster.VERBOSE = True
		log.bwlogger.setLevel( log.VERBOSE )

	if options.uid == None:
		options.uid = uidmodule.getuid()

	command = "display"
	if args: command = args.pop(0)

	# Some commands need to know about processes running as any user
	if command in ("cinfo", "users", "querytags", "log", "mdlist",
				   "start", "stop", "restart", "load"):
		c = cluster.Cluster.get()
	else:
		c = cluster.Cluster.get( uid = uidmodule.getuid( options.uid ) )

	# If no machines running machined on network, abort now
	if not c.getMachines():
		log.error( "No machines running machined on the network, aborting" )
		return 1

	me = c.getUser( uidmodule.getuid( options.uid ) )

	# We're done with common options now - parse command-specific options
	cmdopt = optparse.OptionParser( USAGE )
	if COMMAND_OPTIONS.has_key( command ):
		for switches, kwargs in COMMAND_OPTIONS[ command ]:
			cmdopt.add_option( *switches, **kwargs )
	options, args = cmdopt.parse_args( args )

	ok = True

	if command == "start" or command == "restart":

		if command == "restart":
			if options.preserve:
				return int( not me.restart() )
			else:
				me.smartStop()

		if args:
			ms = selectMachines( c, args )
			if not ms:
				return 1
		else:
			log.error( "You must pass 'all' to start the server "
					   "on all available machines" )
			showUsage( command )
			return 1

		kw = dict( bots = options.bots,
				   revivers = options.revivers,
				   tags = options.tags )

		return int( not me.start( ms, **kw ) )

	elif command == "startproc":
		(pname, mname) = args[:2]
		machine = me.cluster.getMachine( mname )
		if not machine:
			log.error( "Unknown machine '%s'" % mname )
			showUsage( command )
			return 1

		if len( args ) > 2:
			count = int( args[2] )
		else:
			count = 1

		if me.startProc( machine, pname, count ):
			me.ls()
			return 0
		else:
			return 1

	elif command == "stop":
		ok = me.smartStop()

	elif command == "kill":
		ok = me.stop()

	elif command == "nuke":
		ok = me.stop( messages.SignalMessage.SIGQUIT )

	elif command == "killproc":

		procs = selectProcs( me, args )
		if not procs:
			showUsage( command )
			return 1

		for p in procs:
			p.machine.killProc( p )

	elif command == "restartproc":

		procs = selectProcs( me, args )
		if len( procs ) != 1:
			showUsage( command )
			log.error( "You must select a single process" )
			return 1

		p = procs[0]
		pname = p.name
		m = p.machine

		if not m.killProc( p ):
			log.error( "Couldn't shut down %s", p.label() )
			return 1

		if not m.startProc( pname, me.uid ):
			log.error( "Couldn't restart %s", pname )
			return 1

	elif command == "display":
		if not args:
			me.ls()
		else:
			procs = selectProcs( me, args )
			if not procs:
				return 1
			for p in procs:
				print p
			return 0

	elif command == "cinfo":
		if args:
			ms = selectMachines( c, args )
			if not ms:
				return 1
			for m in ms:
				log.info( m )
		else:
			log.info( c )

	elif command == "netinfo":
		if args:
			ms = selectMachines( c, args )
		else:
			ms = c.getMachines()

		for m in ms:
			print m
			for ifname, stats in \
					filter( lambda (i,s): s, sorted( m.ifStats.items() ) ):
				print "\t%s" % stats
			print "\tloss: %d/%d" % (m.inDiscards, m.outDiscards)

	elif command == "users":
		c.lsUsers()

	elif command == "summary":
		me.lsSummary()

	elif command == "check":
		if not me.serverIsRunning():
			log.info( "Server not running for %s" % me )
			return 1
		else:
			log.info( "Server running for %s" % me )
			return 0

	elif command == "save":
		if not args:
			log.error( "You must supply a filename!" )
			showUsage( command )
			return 1
		filename = args.pop(0)
		return int( not me.saveToXML( filename ) )

	elif command == "load":
		if not args:
			log.error( "You must supply a filename!" )
			showUsage( command )
			return 1
		filename = args.pop(0)
		return int( not me.startFromXML( filename ) )

	elif command == "watch" or command == "get":

		if len( args ) < 1:
			log.error( "Insufficient arguments" )
			showUsage( command )
			return 1

		# Passing no watcher path means 'give me the root directory'
		if len( args ) == 1:
			args.append( "" )

		procs = selectProcs( me, args[:-1] )
		if not procs:
			log.error( "No processes selected!" )
			return 1

		for p in procs:
			wd = p.getWatcherData( args[-1] )
			util.printNow( "%-12s on %-8s -> " % (p.label(), p.machine.name) )
			if wd.isDir():
				print "<DIR>"
				for c in wd:
					print "\t%s" % c
			else:
				print wd

		return 0

	elif command == "set":

		if len( args ) < 3:
			log.error( "Insufficient arguments" )
			showUsage( command )
			return 1

		procs = selectProcs( me, args[:-2] )
		if not procs:
			log.error( "No processes selected!" )
			return 1

		path, value = args[-2:]

		status = 0
		for p in procs:
			if not p.setWatcherValue( path, value ):
				log.error( "Failed to set new value for %s (%s)",
						   p.name, p.machine.name )
				status = 1

		return status

	elif command == "pyconsole":

		lock = options.lock_cells

		# Connect to entity
		if options.entity:
			m = re.search( "(cell|base):(\d+)", options.entity )
			if not m:
				log.error( "You must pass (cell|base):<ID> to --entity, "
						   "e.g. --entity cell:2407" )
				showUsage( command )
				return 1

			app = m.group( 1 ) + "app"
			entityID = int( m.group( 2 ) )

			# Find the entity
			log.info( "Scanning %ss for entity %d ...", app, entityID )
			for p in me.getProcs( app ):
				log.info( "Scanning %s ...", p.label() )
				wdir = p.getWatcherData( "entities" )
				for ent in wdir:
					try:
						eid = int( ent.name )
					except ValueError:
						continue
					if eid == entityID:
						log.info( "Entity %d found on %s", eid, p.label() )
						return int( not runscript( [p], lockCells = lock ) )

			log.error( "Entity %d not found on any running %s", entityID, app )
			return 1

		# Connect to process
		else:
			procs = selectProcs( me, args )
			if not procs:
				log.error( "No processes selected!" )
				showUsage( command )
				return 1

			if len( procs ) > 1:
				log.error( "`pyconsole` only supports connecting to a single "
						   "process" )
				return 1

			return int( not runscript( procs, lockCells = lock ) )


	elif command == "runscript":

		# If a .py file is in the arg list, it is the python script
		script = None
		for a in args:
			if a.endswith( ".py" ):
				script = open( a ).read()
				args.remove( a )
				break

		procs = selectProcs( me, args )
		if not procs:
			log.error( "No processes selected!" )
			showUsage( command )
			return 1

		return int( not runscript( procs, script,
								   lockCells = options.lock_cells ) )


	elif command == "querytags":

		if not args or options.all:
			cat, machines = None, selectMachines( c, args )
		elif options.list:
			cat, machines = "", selectMachines( c, args )
		else:
			cat, machines = args[0], selectMachines( c, args[1:] )

		c.queryTags( cat, machines )

		# Displaying all tags
		if options.all or cat is None:
			for m in machines:
				for cc, tags in m.tags.items():
					print "%-20s %s" % ("%s/%s:" % (m.name, cc),
										" ".join( tags ))

		# Displaying list of categories
		elif options.list or cat == "":
			for m in machines:
				if m.tags:
					print "%-10s %s" % (m.name+":", " ".join( m.tags.keys() ))

		# Displaying specific categories
		else:
			for m in machines:
				if m.tags.has_key( cat ):
					print "%-10s %s" % (m.name+":", " ".join( m.tags[ cat ] ))

		return 0


	# Get machines running machined
	elif command == "mdlist":

		ms = selectMachines( c, args )

		if options.ip:
			log.info( options.delim.join(
				sorted( [m.ip for m in ms], util.cmpAddr ) ) )
		else:
			log.info( options.delim.join( sorted( [m.name for m in ms] ) ) )

		return 0


	elif command == "flush":
		ms = selectMachines( c, args )
		ok = True

		for m in ms:
			ok = m.flushMappings() and ok

		return int( not ok )

	elif command == "checkring":
		return int( not cluster.Cluster.checkRing() )


	elif command == "log":

		if not args:
			log.error( "You must supply a log message" )
			showUsage( command )
			return 1

		ms = selectMachines( c, args[1:] )
		loggers = reduce( operator.concat,
						  [m.getProcs( "message_logger" ) for m in ms] )

		for l in loggers:
			l.sendEvent( args[0], me, options.severity )
			log.info( "Sent to %s", l )

		return 0


	elif command == "pyprofile":

		ps = selectProcs( me, args )

		if ps:
			return int( not pyprofile( me, ps, options.time, options.limit,
									   options.cleanup, options.output,
									   options.callers, options.callees,
									   options.login_as_server_user,
									   options.port ) )

		else:
			log.error( "You must specify at least one process to profile" )
			showUsage( command )
			return 1


	elif command == "cprofile":

		procs = selectProcs( me, args )

		if procs:
			return int( not cprofile(
				me, procs, options.time, options.sort, options.limit ) )

		else:
			log.error( "You must specify at least one process to profile" )
			showUsage( command )
			return 1

	elif command == "eventprofile":
		procs = selectProcs( me, ["cellapps"] )

		if procs:
			return int( not eventprofile( procs, options.time, options.sort ) )
		else:
			log.error( "No cellapps, or not server running" )
			return 1


	else:
		opt.print_help()
		log.error( "Unrecognised command: %s", command )
		return 1

	if ok:
		return 0
	else:
		return 1


def showUsage( topic ):

	m = re.search( "\n  (%s.*?)\n  \w" % topic, USAGE, re.M | re.S )
	if not m:
		m = re.search( "\n  (%s.*)\n" % topic, USAGE, re.M | re.S )

	log.info( "\nusage: %s %s\n", os.path.basename( sys.argv[0] ), m.group(1) )


def selectMachines( cluster, args ):

	if not args or args == ["all"]:
		return cluster.getMachines()

	machines = set()

	for a in args:

		# Catch glob() patterns
		if re.search( "[\*\?\[]", a ):
			found = False
			for m in cluster.getMachinesIter():
				if fnmatch.fnmatch( m.name, a ):
					machines.add( m )
					found = True

			if not found:
				log.error( "No machines match pattern '%s'", a )
				return []

		# Catch version:n
		elif a.startswith( "version:" ):
			version = int( a.split( ":" )[1] )
			for m in cluster.getMachinesIter():
				if m.machinedVersion == version:
					machines.add( m )

		# Catch group:g
		elif a.startswith( "group:" ):
			group = a.split( ":" )[1]
			cluster.queryTags( "Groups" )
			found = False
			for m in cluster.getMachinesIter():
				if "Groups" in m.tags and group in m.tags[ "Groups" ]:
					machines.add( m )
					found = True

			if not found:
				log.error( "No machines in group '%s'", group )
				return []

		# Catch :noprocs:
		elif a == ":noprocs:":
			for m in cluster.getMachinesIter():
				if not m.getProcs():
					machines.add( m )

		# Must be an exact hostname or IP address now
		else:
			m = cluster.getMachine( a )
			if m:
				machines.add( m )
			else:
				log.error( "Unknown machine: %s", a )
				return []

	return sorted( machines )


def selectProcs( me, args ):

	procs = set()

	for a in args:

		# Catch 'cellapp1', so 'cellapp{1..5}' brace expansion will work :)
		m = re.match( "([a-z]+)(\d)$", a )
		if m:
			p = me.getProcExact( "%s%02d" % (m.group(1), int( m.group(2) ) ) )
			if p:
				procs.add( p )
				continue
			else:
				log.error( "No process matching '%s'", a )
				return None

		# Catch 'cellapp01'
		if re.match( "[a-z]+\d\d+$", a ):
			p = me.getProcExact( a )
			if p:
				procs.add( p )
				continue
			else:
				log.error( "No process matching '%s'", a )
				return None

		# Catch 'cellapps'
		if re.match( "(cellapps|baseapps|loginapps|bots)", a ):
			if a == "bots":
				ps = me.getProcs( a )
			else:
				ps = me.getProcs( a[:-1] )
			if ps:
				procs.update( ps )
				continue
			else:
				log.error( "No process matching '%s'", a )
				return None

		# Catch 'cellapp'
		if re.match( "(cellapp|baseapp|loginapp)", a ):
			ps = me.getProcs( a )
			if len( ps ) == 1:
				procs.update( ps )
				continue
			elif not ps:
				log.error( "No process matching '%s'", a )
				return None
			else:
				log.error( "There is more than one %s", a )
				log.error( "Please be more specific, or use '%ss'", a )
				return None

		# Catch singleton processes
		if a in cluster.Process.WORLD_PROCS:
			p = me.getProc( a )
			if p:
				procs.add( p )
				continue
			else:
				log.error( "No process matching '%s'", a )
				return None

		# Catch machine:PID
		m = re.match( "(\w+):(\d+)", a )
		if m:
			machine = me.cluster.getMachine( m.group( 1 ) )
			if not machine:
				log.error( "Unknown machine: %s", m.group( 1 ) )
				return None

			pid = int( m.group( 2 ) )
			p = machine.getProc( pid )
			if p:
				procs.add( p )
				continue
			else:
				log.error( "No process with PID %d on %s", pid, machine.name )
				return None

		# Catch 'all'
		if a == "all":
			procs.update( me.getProcs() )
			continue

		log.error( "No process matching '%s'", a )
		return None

	return sorted( procs )


# Trivial subclassing of telnetlib.Telnet to allow read_until() for regexs
class RegexTelnet( telnetlib.Telnet ):

	def __init__( self, *args, **kw ):
		telnetlib.Telnet.__init__( self, *args, **kw )

	def read_until( self, patt ):

		# Make it so the patt can only match at the end of the string
		regex = re.compile( patt + "$" )

		buf = StringIO()
		while not regex.search( buf.getvalue() ):
			buf.write( self.read_some() )

		return buf.getvalue()


def runscript( procs, script = None, lockCells = False ):

	if not procs:
		return False

	# If we are talking to cellapps, lock cells
	if lockCells:
		log.info( "Locking cells ..." )
		procs[0].user().getProc( "cellappmgr" ).shouldOffload( False )
		time.sleep( 0.5 )

	try:

		# If we are interactively connecting to a single app, use the real telnet
		# program so that non line-based input (i.e. 'up arrow') works
		if len( procs ) == 1 and not script:
			p = procs[0]
			port = int( p.getWatcherValue( "pythonServerPort" ) )
			return not os.system( "telnet %s %s" % (p.machine.ip, port) )

		conns = {}
		for p in procs:
			port = int( p.getWatcherValue( "pythonServerPort" ) )
			conns[ RegexTelnet( p.machine.ip, port ) ] = p

		if script is None:
			script = sys.stdin
		else:
			script = StringIO( script )

		# Macro to read server output
		def slurp( conn, script, line = "", silent = False ):

			s = conn.read_until( "(>>>|\.\.\.) " ).replace( line, "", 1 )
			more = s.endswith( "... " )

			if silent:
				return more

			if len( conns ) > 1 or script != sys.stdin:
				s = s[:-4]

			if not s:
				return more

			if len( conns ) > 1:
				s = "%-12s%s" % (conns[ conn ].label() + ":", s)

			util.printNow( s )
			return more


		# Flush initial prompts
		for conn in conns:
			slurp( conn, script,
				   silent = len( procs ) > 1 or script != sys.stdin )

		# Interact
		more = False
		connOrder = sorted( conns.items(), key = lambda (c,p): p )
		while True:

			line = re.sub( "\n$", "\r\n", script.readline() )
			line = re.sub( "\t", "    ", line )
			if not line:
				break

			for conn, p in connOrder:
				conn.write( line )
				more = slurp( conn, script, line )

		# Flush final part of output if the script left the interpreter wanting
		# a final newline (i.e. prompt is stuck on "... ")
		if more:
			for conn, _ in connOrder:
				conn.write( "\r\n" )
				slurp( conn, script, "\r\n" )


	# If we are talking to cellapps, unlock cells
	finally:

		if lockCells:
			procs[0].user().getProc( "cellappmgr" ).shouldOffload( True )
			if script == sys.stdin:
				log.info( "" )
			log.info( "Unlocked cells" )

	return True


def pyprofile( user, procs, secs, limit, cleanup, output, callers, callees,
			   switchLogin, sshPort ):
	"""
	Implements the 'pyprofile' command.
	"""

	# How many profiles we want to generate
	numProfs = len( procs )

	# Generate profile filename template
	now = time.strftime( "%Y-%m-%d-%H:%M:%S" )
	fname = "/tmp/%s-%%s-%s.prof" % (user.name, now )

	# Mapping of process to profile filename
	fnames = dict( [(p, fname % p.label()) for p in procs] )

	for p in procs[:]:

		# Make sure this process supports profiling
		if not p.getWatcherData( "pythonProfile" ):

			log.warning( "%s doesn't support Python profiling, skipping",
						 p.label() )

			procs.remove( p )
			continue


		# Set the filename
		if not p.setWatcherValue( "pythonProfile/filename", fnames[ p ] ):

			log.error( "Couldn't set profile filename on %s, skipping",
					   p.label() )

			procs.remove( p )
			continue

		# Enable profiling
		if not p.setWatcherValue( "pythonProfile/running", "true" ):

			log.error( "Couldn't enable profiling on %s, skipping", p.label() )
			procs.remove( p )
			continue

	# Let the profiles run
	log.info( "Letting profiles run for %.1f seconds ...", secs )
	aborted = False
	try:
		time.sleep( secs )
	except KeyboardInterrupt:
		log.info( "Aborted. Disabling profiling on all processes." )
		aborted = True

	# Disable profiling on all procs
	for p in procs[:]:

		if not p.setWatcherValue( "pythonProfile/running", "false" ):
			log.error( "Couldn't disable profiling on %s, skipping", p.label() )
			procs.remove( p )
			continue

	if aborted:
		return False

	# Copy profiles back to the local machine
	for p in procs:
		if switchLogin:
			os.system( "scp -P %s %s@%s:%s %s" %
					   (sshPort, user.name, p.machine.name,
					   fnames[ p ], fnames[ p ]) )
		else:
			os.system( "scp -P %s %s:%s %s" %
					   (sshPort, p.machine.name, fnames[ p ], fnames[ p ]) )

	if not output:
		output = "/tmp/prof-summary-%s" % now
		log.info( "Profile summary saved to %s", output )

	# Because hotshot sucks and doesn't expose the full pstats API, I have
	# to hack stdout to avoid dumping straight to stdout
	stdout = os.dup( 1 )
	fd = os.open( output, os.O_WRONLY | os.O_CREAT, 0644 )
	os.dup2( fd, 1 )

	# Read profile data and write summary
	import hotshot.stats
	for p in procs:

		print "\n*** %s ***\n" % p.label()
		prof = hotshot.stats.load( fnames[ p ] )
		prof.strip_dirs()

		prof.sort_stats( "time" )
		prof.print_stats( limit )

		prof.sort_stats( "cumulative" )
		prof.print_stats( limit )

		if callers:
			prof.sort_stats( "time" )
			prof.print_callers( limit )

		if callees:
			prof.sort_stats( "cumulative" )
			prof.print_callees( limit )

	# Restore stdout
	os.close( fd )
	os.dup2( stdout, 1 )

	# Dump summary to stdout
	sys.stdout.write( open( output ).read() )

	# Cleanup profile data if necessary
	if cleanup:

		for p in procs:

			os.unlink( fnames[ p ] )

			if switchLogin:
				os.system( "ssh %s@%s 'rm %s'" %
						   (user.name, p.machine.name, fnames[ p ]) )
			else:
				os.system( "ssh %s 'rm %s'" % (p.machine.name, fnames[ p ]) )

	return len( procs ) == numProfs


def cprofile( user, procs, secs, sortkey, limit ):
	"""
	Implements the 'cprofile' command.
	"""

	# Object to read a single profile detail directory
	class Profile( object ):

		def __init__( self, proc, wd, stamps ):

			self.name = wd.name
			self.count = int( wd.getChild( "count" ).value )
			self.cumTime = int( wd.getChild( "sumTime" ).value ) / stamps
			self.cumTimePerCall = 0

			self.calcTimePerCall()


		def calcTimePerCall( self ):

			if self.count:
				self.cumTimePerCall = int( self.cumTime / self.count * 1000000 )
			else:
				self.cumTimePerCall = 0


		def delta( self, prev ):

			self.count -= prev.count
			self.cumTime -= prev.cumTime
			self.calcTimePerCall()


	# Object to sample all profiles at a specific point in time
	class Sample( object ):

		def __init__( self, proc ):

			self.proc = proc
			self.profiles = {}

			self.stampsPerSec = float(
				proc.getWatcherValue( "stats/stampsPerSecond" ) )

			# Iterate over all profiles
			for prof in proc.getWatcherData( "profiles/details" ):
				self.profiles[ prof.name.lower() ] = \
							   Profile( proc, prof,	self.stampsPerSec )

			# Calculate total running time
			self.spareTime = float(
				proc.getWatcherValue( "nub/timing/totalSpareTime" ) ) / \
				self.stampsPerSec

			self.runningTime = (int(
				proc.getWatcherValue( "stats/runningTime" ) ) / \
				self.stampsPerSec) - self.spareTime

		# Subtract profile data from a previous sample to turn this sample into
		# a delta.
		def delta( self, prev ):

			for name in self.profiles:
				self.profiles[ name ].delta( prev.profiles[ name ] )

			self.runningTime -= prev.runningTime
			self.spareTime -= prev.spareTime


	# Take the initial samples, then wait.
	samples = dict( [(p, Sample( p )) for p in procs] )

	print "Waiting %.1f secs for sample data ..." % secs
	time.sleep( secs )

	# Take deltas against the initial samples.
	for proc in samples:
		newSample = Sample( proc )
		newSample.delta( samples[ proc ] )
		samples[ proc ] = newSample

	# Display profiles over the intervening period for each process
	for proc, sample in samples.items():

		# Calculate max width of profile name
		width = reduce( lambda x, y: max( x, y ),
						[len( s ) for s in sample.profiles] )

		template = "%%-%ds " % width
		template += "%8d | %6.3fs %6dus %6.2f%%"

		print "\n*** %s ***\n" % proc.label()

		# Display profiles
		numDisplayed = 0

		for name, profile in \
			sorted( sample.profiles.items(),
					key = lambda (n,p): -getattr( p, sortkey ) ):

			if profile.name != "running":

				print template % (profile.name, profile.count,
								  profile.cumTime, profile.cumTimePerCall,
								  100 * profile.cumTime / sample.runningTime)

				numDisplayed += 1
				if numDisplayed >= limit:
					break

		print "\nTotal running time: %.3fs (%.3fs spare)" % \
			  (sample.runningTime, sample.spareTime)

	return True

class EventStat( object ):
	def __init__( self, name, count, size, period, bandwidthMakesSense ):
		self.name = name
		self.count = count
		self.size = size
		self.period = period
		self._bandwidthMakesSense = bandwidthMakesSense

	def avgSizePerCount( self ):
		return float( self.size ) / float( self.count )

	def avgBytesPerSecond( self ):
		return float( self.size ) / float( self.period )

	def __str__( self ):
		format = "%-30s %10d %10d %10.03f"
		args = (self.name, self.count, self.size, self.avgSizePerCount())
		if self._bandwidthMakesSense:
			format += " %10.03f"
			args += (self.avgBytesPerSecond(),)
		out = ""
		if len( self.name ) > 30:
			args = ("",) + args[1:]
			out = self.name + "\n"

		out += format % args
		return out



EVENTPROFILE_TYPES = ["publicClientEvents", "privateClientEvents",
		"totalPublicClientEvents"]


EVENTPROFILE_SORT_TYPES = {
	"bandwidth":
		lambda y, x: cmp( x.avgBytesPerSecond(),  y.avgBytesPerSecond() ),
	"count":
		lambda y, x: cmp( x.count, y.count ),
	"totalsize":
		lambda y, x: cmp( x.size, y.size ),
	"avgsize":
		lambda y, x: cmp( x.avgSizePerCount() > y.avgSizePerCount() )
}

EVENTPROFILE_HAS_BANDWIDTH = ['totalPublicClientEvents', 'privateClientEvents']

def eventprofile( procs, secs, sortType="bandwidth" ):
	# turn event tracking profiling off then on to reset counters

	if sortType not in EVENTPROFILE_SORT_TYPES:
		print "Not a valid sort type: %s" % sortType
		return False

	for p in procs:
		for eventType in EVENTPROFILE_TYPES:
			p.setWatcherValue( eventType + "/enabled", False )
	for p in procs:
		for eventType in EVENTPROFILE_TYPES:
			p.setWatcherValue( eventType + "/enabled", True )

	print "Waiting %.1f secs for sample data ..." % secs
	time.sleep( secs )

	# turn them off
	for p in procs:
		for eventType in EVENTPROFILE_TYPES:
			p.setWatcherValue( eventType + "/enabled", False )

	# get the counts and sizes
	for p in procs:
		print "**** %s ****" % p.label()
		for eventType in EVENTPROFILE_TYPES:
			eventCounts = {}
			eventSizes = {}

			eventStats = []
			counts = p.getWatcherData( eventType + "/counts" ).getChildren()
			for countData in counts:
				eventCounts[countData.name] = int( countData.value )

			sizes = p.getWatcherData( eventType + "/sizes" ).getChildren()
			for sizeData in sizes:
				eventSizes[sizeData.name] = int( sizeData.value )

			for name in eventCounts:
				count = eventCounts[name]
				size = eventSizes[name]
				eventStats.append( EventStat( name, count, size, secs,
					eventType in EVENTPROFILE_HAS_BANDWIDTH ) )

			eventStats.sort(cmp=EVENTPROFILE_SORT_TYPES[sortType])

			print "\nEvent Type: %s\n" % eventType


			if eventStats:
				print "%-30s %10s %10s %10s %10s\n" % \
					("Name", "#", "Size", "AvgSize", "Bandwidth")
				for stat in eventStats:
					print str( stat )
			else:
				print "No events"

			print

if __name__ == "__main__":
	sys.exit( main() )
