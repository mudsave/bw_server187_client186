"""
This is a wrapper around the C++ BWLog object and should be used where possible
instead of trying to access the raw C++ API
"""

import os
import time
import ConfigParser

import bwsetup; bwsetup.addPath( ".." )
import bwlog
from install import install_tools
from pycommon import util
from pycommon import cluster
from pycommon import log


class MessageLog( object ):

	def __init__( self, root = None ):
		self.root = root or self.getDefaultLogdir()
		self.log = bwlog.BWLog( self.root )


	@classmethod
	def getConfig( self ):

		# Try global one first
		try:
			globalConf = install_tools.readConf()
			mldir = globalConf[ "location" ] + "/message_logger"

		# Otherwise fall back to the directory this file lives in
		except KeyError:
			mldir = bwsetup.appdir
		except ConfigParser.NoSectionError:
			mldir = bwsetup.appdir

		config = ConfigParser.SafeConfigParser()
		config.read( "%s/message_logger.conf" % mldir )
		return (config, mldir)


	@classmethod
	def getDefaultLogdir( self ):
		(config, mldir) = self.getConfig()
		logdir = config.get( "message_logger", "logdir" )

		if not logdir.startswith( "/" ):
			logdir = mldir + "/" + logdir

		return logdir

	# --------------------------------------------------------------------------
	# Section: C++ wrappers
	# --------------------------------------------------------------------------

	def getUsers( self ):
		return self.log.getUsers()

	def getComponentNames( self ):
		return self.log.getComponentNames()

	def getHostnames( self ):
		return self.log.getHostnames()

	def getUserLog( self, uid ):
		return UserLog( self.log.getUserLog( uid ) )

	def fetch( self, **kw ):
		return Query( self.log.fetch( **kw ) )

	# --------------------------------------------------------------------------
	# Section: Additional functionality
	# --------------------------------------------------------------------------

	# Check the message_logger log directory for the pid file and then
	# check whether that process exists on the system.
	def isRunning( self ):

		try:
			# If the PID file exists, check if the process is running
			pidfile = self.root + "/pid"
			if os.path.exists( pidfile ):
				pfp = open( pidfile )
				pid = pfp.readline().strip()
				pfp.close()

				if os.path.exists( "/proc/" + pid ):
					return True
		except:
			pass

		return False

	def getUIDs( self ):
		return self.getUsers().values()

	def dump( self, query, flags = bwlog.SHOW_ALL ):
		if query.inReverse():
			lines = []
			for result in query:
				lines.append( result.format( flags ) )
			for line in lines[::-1]:
				print line,
		else:
			for result in query:
				print result.format( flags ),

	def collect( self, *args, **kw ):
		query = self.fetch( *args, **kw )
		results = list( query )
		if query.inReverse(): results.reverse()
		return (query, results)

	def format( self, flags = bwlog.SHOW_ALL, *args, **kw ):
		query = self.fetch( *args, **kw )
		results = [result.format( flags ) for result in query]
		if query.inReverse(): results.reverse()
		return (query, results)

	def histogram( self, group, *args, **kw ):
		query = self.fetch( *args, **kw )
		groups = {}

		for result in query:
			groups[ getattr( result, group ) ] = \
					groups.get( getattr( result, group ), 0 ) + 1

		pairs = groups.items()
		pairs.sort( key = lambda (k,v): v, reverse=True )
		return (query, pairs)


	def fetchContext( self, kwargs, max = 20, period = None,
					  contextTimeout = 3.0 ):
		"""
		Utility method useful for implementing `tail -f` like functionality,
		where you want to have some prior context for a query before returning
		the up-to-date results.  At the moment this only supports context that
		is backwards in time, i.e. you can't get context for a query that is
		going to run in the backwards direction.

		If you want to restrict the time window the context can be extracted
		from, pass 'period' as something like '-3600' (for 1 hour back in time).

		The return value is a list of results and a query positioned immediately
		after the last returned result.
		"""

		kw = kwargs.copy()
		kw[ "period" ] = period or "to beginning"

		# Cap time spent searching for context for potentially sparse queries
		def fetchContextTimeout( query ):
			raise IOError( "Context lookup took longer than %f seconds" %
						   contextTimeout )

		query = self.fetch( **kw )
		query.setTimeout( contextTimeout, fetchContextTimeout )
		resumePos = query.tell()
		results = reversed( query.get( max ) )

		if results:

			# Start new query but jump over first position since we've already
			# searched it
			kwargs[ "startaddr" ] = resumePos
			query = self.fetch( **kwargs )
			query.step( bwlog.FORWARDS )

			return (results, query)

		# If we couldn't even get one line of context, just return original search
		else:
			return ([], self.fetch( **kwargs ))


class Query( object ):

	def __init__( self, query ):
		self.query = query

	# --------------------------------------------------------------------------
	# Section: C++ wrappers
	# --------------------------------------------------------------------------

	def get( self, *args ):
		return self.query.get( *args )

	def inReverse( self ):
		return self.query.inReverse()

	def getProgress( self ):
		return self.query.getProgress()

	def resume( self ):
		return self.query.resume()

	def tell( self, *args ):
		return self.query.tell( *args )

	def seek( self, *args ):
		return self.query.seek( *args )

	def step( self, *args ):
		return self.query.step( *args )

	def setTimeout( self, *args ):
		return self.query.setTimeout( *args )

	def __iter__( self ):
		return self.query.__iter__()

	# --------------------------------------------------------------------------
	# Section: Additional Functionality
	# --------------------------------------------------------------------------

	def hasMoreResults( self ):
		seen, total = self.getProgress()
		return seen < total

	def waitForResults( self, timeout = 0.5 ):
		while not self.hasMoreResults():
			time.sleep( timeout )
			self.resume()


class UserLog( object ):

	def __init__( self, userLog ):
		self.userLog = userLog
		self.uid = userLog.uid
		self.username = userLog.username

	def getSegments( self ):
		return [Segment( *tup ) for tup in self.userLog.getSegments()]

	def getComponents( self ):
		return [Component( self, *tup )	for tup in self.userLog.getComponents()]

	def getEntry( self, entryAddress ):
		return self.userLog.getEntry( entryAddress )


	def getLastServerStartup( self ):

		for component in self.getComponents()[::-1]:

			entry = component.getFirstEntry()

			# This can happen if the segment has been rolled away.  If it has,
			# then we don't need to keep searching through the rest of the
			# components, just start from the beginning of the log, since it
			# must be later than the last server restart
			if entry is None:
				break

			if component.name == cluster.MESSAGE_LOGGER_NAME and \
				   entry.message == "Starting server":
				return dict( entry = entry, addr = component.firstEntry )

		return None

class Segment( object ):

	FMT = "%-23s %10s %16s %8s"

	def __init__( self, suffix, start, end, nEntries, entriesSize, argsSize ):
		self.suffix = suffix
		self.start = start
		self.end = end
		self.nEntries = nEntries
		self.entriesSize = entriesSize
		self.argsSize = argsSize

	def __str__( self ):
		return self.FMT % \
			   (self.suffix, util.fmtSeconds( self.end - self.start ),
				self.nEntries,
				util.fmtBytes( self.entriesSize + self.argsSize ))


class Component( object ):

	def __init__( self, userLog, name, pid, id, firstEntry ):
		self.userLog = userLog
		self.name = name
		self.pid = pid
		self.id = id
		self.firstEntry = firstEntry

	def __str__( self ):
		return "%-12s (pid:%d) (id:%d)" % (self.name, self.pid, self.id)

	def getFirstEntry( self ):
		return self.userLog.getEntry( self.firstEntry )
