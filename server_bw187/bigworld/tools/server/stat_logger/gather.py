import threading
import select
import socket
import time
import struct
import re
import sys
import Queue
import math
import logging
import traceback
import math

# local modules
import constants
import utilities

# pycommon modules
import bwsetup
bwsetup.addPath( ".." )
from pycommon import messages, cluster

# Logging module
log = logging.getLogger( "stat_logger" )

# ------------------------------------------------------------------------------
# Section: Helper classes for storing statistics
# ------------------------------------------------------------------------------

class StatRow:
	"""
	Represents one row of statistics.
	"""

	def __init__( self, tick, values ):
		""" Constructor. """
		self.tick = tick
		self.values = values

	def setValue( self, pos, value ):
		"""
		Set the pos'th column in this statistic row to the given value.
		Extends the row automatically if pos is larger than the number
		of values held.
		"""
		if (len(self.values) - 1) < pos:
			diffSize = pos - len(self.values) + 1
			log.verbose( "Extending row from %d to %d",
				len(self.values), len(self.values) + diffSize)
			self.values.extend( [None] * diffSize )

		self.values[pos] = value

class Statistics:
	"""
	A group of collected statistic rows. This group of rows is intended to be
	inserted into the logging database.
	"""
	def __init__( self, statLength ):
		"""
		Constructor.
		"""
		self.statLength = statLength
		self.rows = []
		self.lastHeard = None

	def getCurrentStatRow( self, tick ):
		"""
		Retrieve the statistics row for the given tick.
		"""
		if len(self.rows) == 0 or self.rows[-1].tick != tick:
			return self.newStatRow( tick, self.statLength )
		else:
			return self.rows[-1]

	def newStatRow( self, tick, rowSize ):
		"""
		Create and append a new statistics row, and return it.
		"""
		newStatRow = StatRow( tick, [None] * rowSize )
		self.rows.append( newStatRow )
		return newStatRow

# -----------------------------------------------------------------------------
# Section: Gathering exception class
# -----------------------------------------------------------------------------
class GatherError( Exception ):
	"""
	Gather error.
	"""
	pass


# -----------------------------------------------------------------------------
# Section: Gathering thread
# -----------------------------------------------------------------------------

class Gather( threading.Thread ):
	"""
	This is a thread responsible for gathering statistics.
	"""

	# Daemon log mark output period. Mark every hour.
	MARK_PERIOD_SECS = 3600

	def __init__( self, options, args, prefTree, dbManager ):
		"""
		Constructor.
		"""
		threading.Thread.__init__(self, name="Gather" )

		# Variables only used for debugging
		self.lastUpdate = 0

		# Update interval, pulled from options file
		self.sampleTickInterval = options.sampleTickInterval

		# Don't print the tick rotater if we're logging to a file
		self.printRotater = (args.stdoutFile == None)

		# Cluster object (from pycommon) which does the heavyweight work
		self.cluster = cluster.Cluster()

		# Thread locks for when we shut down
		self.finishLogLock = threading.Lock()
		self.finishLog = False

		# Machine statistics: [Machine object] -> [Statistics object]
		self.machineStats = {}

		# Process statistics: [Process object] -> [Statistics object]
		self.processStats = {}

		# User statistics: [Set of user objects]
		self.userStats = set()

		# Database manager
		self.dbManager = dbManager

		# PrefTree
		self.prefTree = prefTree

		# ProcessName to process type
		self.processNameToPref = {}

		# Stat pref to lambda
		self.statPrefToLambda = {}

		# Process database ids (Machines use ip integer as their id)
		self.dbIds = {}

		# SQL statements
		self.machineLogSql = None
		self.machineConsolidateSql = None
		self.processLogSql = {}
		self.processConsolidateSql = {}

		self.numMachineStats = 0
		self.numProcessStats = {}

		# Preprocess Preference tree
		self.enumeratedStats = {}
		self.enumeratedMachineStats = {}
		self.watcherStats = {}
		self.nonWatcherStats = {}
		self.valueAtToStatNames = {}
		self.windowPrefs = []
		self.preprocessPrefTree( self.prefTree )

		# Special profiling flag
		self.profiling = False

		# last time we outputted a mark, output mark after one period after init
		self.lastMark = time.time()

		# Guards against having the computer clock set backwards
		self.maxTime = None
		self.timeCatchup = False

		# Rotating thingy
		self.rotateChars = "|/-\\"
		self.indicatePos = 0

	def preprocessPrefTree( self, prefTree ):
		self.enumeratedStats = {}
		self.enumeratedMachineStats = {}
		self.watcherStats = {}
		self.nonWatcherStats = {}

		baseWindowDbId = self.prefTree.windowPrefs[0].dbId
		# Begin iterating through process categories
		for procPref in prefTree.iterProcPrefs():

			# Dictionary for holding enumerated statistic values
			enumDict = {}
			self.enumeratedStats[procPref.name] = enumDict

			# Dictionary for holding watcher statistic directives
			watcherDict = {}
			self.watcherStats[procPref.name] = watcherDict

			# Dictionary mapping stat values to stat pref names that
			# have them as their valueAt
			valueAtToNames = {}
			self.valueAtToStatNames[procPref.name] = valueAtToNames

			# Dictionary for holding non-watcher statistic directives
			nonWatcherDict = {}
			self.nonWatcherStats[procPref.name] = nonWatcherDict

			# Get a list of statistics under this process category
			procPrefStats = list(procPref.iterAllStatPrefs())

			# Count the number of statistics (used for initialising list sizes
			# when storing statistics)
			self.numProcessStats[ procPref.name ] = len( procPrefStats )

			#print "Num stats for %s: %d" % \
			#	(procPref.name, len( procPrefStats ) )
			for i, statPref in enumerate(procPrefStats):

				# Enumerate each statistic under the process category
				# This creates a mapping of [statistic name]-->[position in row]
				enumDict[statPref.name] = i

				if not valueAtToNames.has_key( statPref.valueAt ):
					valueAtToNames[statPref.valueAt] = []
				valueAtToNames[statPref.valueAt].append( statPref.name )

				# Identify each statistic type as a watcher value or class
				# member value
				# Watcher values are identified as "/".
				# Anything else is a class member value (i.e. obtained directly
				# from the Process class)
				if statPref.valueAt[0] == '/':
					watcherDict[statPref.name] = statPref
				else:
					nonWatcherDict[statPref.name] = statPref

			# For each process category, collect the column names and
			# generate a SQL INSERT template.
			columnNames = ["tick", "process"]
			aggregateColumnNames = []
			for p in procPrefStats:
				columnNames.append( p.columnName )
				aggregateColumnNames.append( p.consolidateColumn() )

			self.processLogSql[ procPref.name ] = \
				"INSERT INTO %s_lvl_%03d(%s) VALUES(%s)" % ( \
					procPref.tableName, baseWindowDbId,
					", ".join( columnNames ),
					", ".join( ['%s'] * len( columnNames ) ) )

			self.processConsolidateSql[ procPref ] = """
				INSERT INTO %%s( %s )
					SELECT
					-- Current tick
					%%s AS currentTick,
					stat.process,
					-- Aggregated columns
					%s
				FROM %%s AS stat
				WHERE
					stat.tick >= %%s AND stat.tick < %%s
				GROUP BY
					stat.process;
				""" % \
					(", ".join( columnNames ),
					", \n".join( aggregateColumnNames ))
			#print "Process sql for %s: %s" % \
			#	(procPref.name, self.processSql[ procPref.name] )


		# Enumerate machine statistics
		# This creates a mapping of [statistic name]-->[position in row]
		for i, msPref in enumerate( prefTree.iterMachineStatPrefs() ):
			self.enumeratedMachineStats[ msPref.name ] = i

		# Collect machine statistic column names and generate a SQL consolidate
		# template and SQL INSERT template
		machineColumnNames = ["tick", "machine"]
		aggregateColumnNames = []
		for m in self.prefTree.iterMachineStatPrefs():
			machineColumnNames.append( m.columnName )
			aggregateColumnNames.append( m.consolidateColumn() )

		self.numMachineStats = len( self.prefTree.machineStatPrefs )

		self.machineLogSql = " INSERT INTO %s_lvl_%03d(%s) VALUES(%s)" % ( \
			constants.tblStatMachines, baseWindowDbId,
			", ".join(machineColumnNames),
			", ".join( ['%s'] * len(machineColumnNames) ) )

		self.machineConsolidateSql = """
				INSERT INTO %%s( %s )
				SELECT
					-- Current tick
					%%s AS currentTick,
					stat.machine,
					-- Aggregated columns
					%s
				FROM %%s AS stat
				WHERE
					stat.tick >= %%s AND stat.tick < %%s
				GROUP BY
					stat.machine;
			""" % \
				(", ".join(machineColumnNames),
				", \n".join( aggregateColumnNames ))

		#print "Machine log SQL: %s" % (self.machineLogSql)
		#print "Machine consolidate SQL: \n%s" % (self.machineConsolidateSql)

		# Get the window preferences
		self.windowPrefs = self.prefTree.windowPrefs

	def guessCurrentTick( self ):
		results = self.dbManager.getFirstTick()

		if results == None:
			nextTick = 1
			nextTickTime = time.time()
		else:
			firstTick, firstTickTime = results
			now = time.time()
			diff = now - firstTickTime

			numTicks = diff / self.sampleTickInterval
			nextTick = firstTick + math.ceil( numTicks )
			nextTickTime = firstTickTime + \
				(float(nextTick) * self.sampleTickInterval)

			#print "First tick %d was at %f" % (firstTick, firstTickTime)
			#print "Now is %f" % (now)
			#print "Time in between = %f, Ticks in between = %f" % \
			#	( diff, numTicks )
			#print "This makes the next tick start at %f" % (nextTickTime)

		return nextTick, nextTickTime

	def run( self ):
		self.profiling = False
		if self.profiling:
			import cProfile
			p = cProfile.Profile()
			p.runcall(self.doGather)
			p.dump_stats("stat_logger.prof")
			log.debug( "Dumped profile stats" )
			self.finishEvent.set()
		else:
			#log.debug( "Not profiling" )
			self.doGather()

		return

	# Main worker function - loops until program termination.
	def doGather( self ):

		# Start the database logging thread
		self.statDumper = StatDumper( self.dbManager.cloneWithNewConnection() )
		self.statDumper.start()

		# Grab the last tick out of the database
		cursor = self.dbManager.getLogDbCursor()
		cursor.execute( "SELECT id, time FROM %s "\
			"ORDER BY time DESC LIMIT 1" % constants.tblStdTickTimes )
		if cursor.rowcount > 0:
			lastTick, lastTime = cursor.fetchone()
			self.maxTime = float( lastTime ) / 1000
			log.info( "Max time: %.3f Now: %.3fs", self.maxTime, time.time() )
		cursor.close()

		# Desired tick time represents the EXACT time at which we wanted the
		# tick to happen...due to overhead, we'll always be logging 1 or 2
		# milliseconds off this time, but at a constant rate
		self.tick, desiredTickTime = self.guessCurrentTick()

		# If this isn't the start of the log, wait a bit
		if self.tick > 1:
			time.sleep( desiredTickTime - time.time() )
			self.drift = time.time() - desiredTickTime

		# Delete old data.  Old data may span multiple ticks, and thus the
		# amount of data to delete will vary.
		self.deleteOldData()

		log.info( "StatLogger ready to collect data." )

		while True:
			startLoop = time.time()

			# Get the current time and log it
			self.tickTime = time.time()

			# Check we haven't gone backwards in time
			if self.maxTime != None and self.tickTime < self.maxTime:
				if not self.timeCatchup:
					log.warning( "Current time %s is lower than maximum recorded " \
						"time of %s (difference of %.3fs)",
						time.ctime( self.tickTime ), time.ctime( self.maxTime ),
						self.maxTime - self.tickTime )
					log.warning( "Has the computer's clock been set backwards?" )
					log.info( "Going to stop collecting data until clock " \
						"has caught up to previous maximum time" )
					self.timeCatchup = True
				else:
					log.info( "Waiting for clock to catch up..." \
						"(%.3fs difference)", self.maxTime - self.tickTime  )
				time.sleep( self.sampleTickInterval )
				continue
			else:
				self.maxTime = self.tickTime

				if self.timeCatchup:
					self.timeCatchup = False
					log.info( "Resuming data collection" )

				self.dbManager.addTickToDb( self.tick, self.tickTime )
				self.collectData()

			#fttime = time.time()
			#log.debug( "Begin consolidation of stats" )
			self.consolidateAll()
			#log.debug( "End consolidation...(Took %f seconds)",
			#	(time.time() - fttime) )

			# Check if the program's been told to finish up,
			# and if so then we make our last logs
				# TODO: Remove
			self.finishLogLock.acquire()
			if self.finishLog:
				self.finishLogLock.release()
				logFinishEvent = self.statDumper.finishUp()
				log.info ( "Waiting to dump remaining stats to the database "\
					"(%d in queue)", self.statDumper.queueSize())
				logFinishEvent.wait()
				if self.profiling:
					log.info( "Gather thread terminating" )
					# Don't trigger finish event here if profiling,
					# since it's triggered after this function returns.
					# Slightly hacky, but only when profiling
				else:
					self.finishEvent.set()
				return
			self.finishLogLock.release()

			# Update tick, desired tick time
			desiredTickTime += self.sampleTickInterval
			#log.debug( "Tick: %d", self.tick )
			self.tick += 1

			#log.debug( "Loop took %f seconds", time.time() - startLoop )

			queueSize = self.statDumper.queueSize()
			#log.info( "Queue size is now: %d" % (queueSize))
			if queueSize > 500:
				log.warning( "Queue size is larger than 500 (now %d)",
					queueSize )

			queryInfo = self.statDumper.getCurrentQueryInfo()
			if queryInfo and queryInfo[1] > 3:
				log.warning(
					"Currently stuck on query: %s (taken %.3fs so far)",
					queryInfo[0], queryInfo[1] )

			# If we've gone overtime...then "skip" ticks
			timeRemaining = desiredTickTime - time.time()
			if timeRemaining < 0:
				loopSkip = math.ceil( (-timeRemaining) /
					self.sampleTickInterval )
				self.tick += loopSkip
				desiredTickTime += loopSkip * self.sampleTickInterval
				log.debug( "Interval went overtime by %fs, skipping %d ticks",
					-timeRemaining, loopSkip )
				log.debug( "Waiting %fs to proceed with tick %d",
					desiredTickTime - time.time(), self.tick )

			# Recalculate timeRemaining to compensate for time spent in
			# previous if statement
			timeRemaining = desiredTickTime - time.time()
			# If we still have time remaining, wait that time amount
			if timeRemaining > 0:
				time.sleep( timeRemaining )

	def deleteOldData( self ):
		""" 
		Delete old data that may span multiple ticks.  The amount of data to
		delete varies.  This is performed only on StatLogger startup.
		"""		

		log.info( "Performing maintenance task...This may take a while." )
		
		# Uses deletion code in consolidateAll method.  No consolidation is 
		# performed.
		log.info( "Deleting un-used statistics." )
		self.consolidateAll( False )

		log.info( "Finished performing maintenance task." )


	def collectData( self ):
		# Refresh cluster
		#fttime = time.time()
		#log.debug( "Begin cluster.refresh()" )
		try:
			self.cluster.refresh( 1 )
		except:
			traceback.print_exc()
		#log.debug( "End cluster.refresh()...(Took %f seconds)",
		#	(time.time() - fttime) )

		# Extract cluster statistics
		#fttime = time.time()
		#log.debug("Begin collect stats (including making watcher requests)...)"
		self.extractClusterStats()
		#log.debug ("End collect stats...(Took %f seconds)",
		#	(time.time() - fttime) )

		# Check for dead processes timing out
		self.checkDeadProcessesAndMachines()

		# Check that the stat dumper thread is still running
		if not self.statDumper.isAlive():
			self.statDumper.raiseDeferredException()
			# If we get here then we have no exception from the stat dumper
			# thread, so raise this fallback exception.
			log.error( "Error: StatDumper thread died unexpectedly, "\
				"stopping Gather thread." )
			raise RuntimeError, "StatDumper died unexpectedly with no exception"

		if self.printRotater:
			sys.stdout.write( "%c\r" % (self.rotateChars[self.indicatePos]) )
			sys.stdout.flush()
			self.indicatePos = (self.indicatePos + 1) % len( self.rotateChars )
		else:
			# print MARK if the mark period has elapsed
			timeNow = time.time()
			if timeNow - self.lastMark > Gather.MARK_PERIOD_SECS:
				log.info( "-- MARK --" )
				self.lastMark = timeNow

		self.logAll()

	def extractClusterStats( self ):
		self.extractMachineStats()
		self.extractProcessStats()
		self.extractUserStats()


	def extractUserStats(self):
		users = self.cluster.getUsers()
		for u in users:
			if u not in self.userStats:
				self.logNewUser(u)

	def extractMachineStats( self ):
		machines = self.cluster.getMachines()

		for m in machines:
			#print "Getting stats from machine %s" % (m)
			try:
				statRow = self.machineStats[m].getCurrentStatRow( self.tick )
			except KeyError:
				statLength = self.numMachineStats
				self.machineStats[m] = Statistics( statLength )
				self.logNewMachine(m)
				statRow = self.machineStats[m].getCurrentStatRow( self.tick )

			for msPref in self.prefTree.iterMachineStatPrefs():
				pos = self.enumeratedMachineStats[ msPref.name ]
				value = self.getMachineStat( m, msPref )
				statRow.setValue( pos, value )
				#print "Getting stat %s for %s" % (msPref.name, m.name)

	def extractProcessStats( self ):
		for procPref in self.prefTree.iterProcPrefs():
			processes = self.cluster.getProcs( procPref.matchtext )

			#print "Got %d %ss" % (len(processes), procPref.name)
			for process in processes:
				try:
					statRow = self.processStats[process].getCurrentStatRow(
						self.tick )
				except KeyError:
					statLength = self.numProcessStats[ procPref.name ]
					self.processStats[process] = Statistics( statLength )
					self.logNewProcess( process )
					statRow = self.processStats[process].getCurrentStatRow(
						self.tick )

				for statPref in \
						self.nonWatcherStats[ procPref.name ].itervalues():
					pos = self.enumeratedStats[ procPref.name ][ statPref.name ]
					value = self.getProcessStat( process, statPref )
					#print "Setting %d value to %f" % (pos, value)
					statRow.setValue( pos, value )

			fttime = time.time()
			self.extractWatcherValues( processes, procPref )
			#print "Getting watcher values for %s took %fs" % \
			#	(procPref.name, time.time() - fttime)


	def getMachineStat( self, machine, machineStatPref ):
		#print "Processing stat: %s" % (machineStatPref.valueAt)

		try:
			func = self.statPrefToLambda[machineStatPref.valueAt]
		except KeyError:
			func = eval("lambda m: m." + machineStatPref.valueAt)
			self.statPrefToLambda[machineStatPref.valueAt] = func

		try:
			result = func( machine )
		# Note here: May need to add more exception catching later
		# if these evals become more complex
		except IndexError:
			return 0.0
		except KeyError:
			return 0.0
		#print "Returned %s!" % (result)
		return result


	def getProcessStat( self, process, statPref ):
		#log.debug( "Processing stat: %s", statPref.valueAt )

		try:
			func = self.statPrefToLambda[statPref.valueAt]
		except KeyError:
			func = eval("lambda p: p." + statPref.valueAt)
			self.statPrefToLambda[statPref.valueAt] = func
			#log.debug("getProcessStat: Eval'ing %s", statPref.valueAt)

		try:
			result = func( process )
		except IndexError:
			return 0.0
		#print "Returned %s!" % (result)
		return result

	def extractWatcherValues( self, processes, procPref ):
		numWatcherPaths = len( self.watcherStats[ procPref.name ] )
		#print "Making watcher query: %s" % (procPref.name)
		if numWatcherPaths == 0:
			return

		watcherPaths = iter( statPref.valueAt[1:]
			for statPref in self.watcherStats[procPref.name].itervalues() )
		watcherResponse = messages.WatcherDataMessage.batchQuery(
			watcherPaths, processes , 0.5 )

		for process, watcherReplyDict in watcherResponse.iteritems():
			#print "%s: %s" % (process, watcherReplies)
			statRow = self.processStats[process].getCurrentStatRow(	self.tick )

			for watcherRequest, watcherReplies in \
					watcherReplyDict.iteritems():
				if len( watcherReplies ) > 1:
					# watcherReplies is a list of
					# (watcherPath, watcherValue) tuples
					# e.g.
					# [('baseAppLoad/min', '0.00192496')]
					#
					# Usually we only get multiple values in watcherReplies
					# if we've made a watcher request for a directory, but
					# at the moment this also happens if a previous
					# watcher response is delayed such that we get
					# more than one response in the loop. watcherReplies
					# then looks like:
					#
					# [('baseAppLoad/min', '0.00192496'),
					#	('baseAppLoad/min', '0.00192501')]
					#
					# So for now we'll just take the last element
					# if that happens.

					log.warning( "ERROR: Watcher request %s to %s " \
						"resulted in multiple replies. Possible delayed " \
						"watcher response to earlier query. " \
						"Only using last value.", watcherRequest, process )
					#log.debug( "Watcher reply list: %s", str(watcherReplies) )
					value = watcherReplies[-1][1]
				else: # not len( watcherReplies) > 1
					value = watcherReplies[0][1]

				valueAtToNames = self.valueAtToStatNames[procPref.name]
				statPrefNames = valueAtToNames["/" + watcherRequest]
				for statPrefName in statPrefNames:
					statPref = self.watcherStats[procPref.name][statPrefName]
					pos = self.enumeratedStats[procPref.name][statPref.name]
					#print "Setting %d value (%s) to %s" %
					#	(pos, watcherPath,value)
					statRow.setValue( pos, value )

	def logNewMachine( self, machine ):
		ipInt = utilities.ipToInt( machine.ip )
		c = self.dbManager.getLogDbCursor()

		# Seach the database first, and grab the name stored in the database
		c.execute( "SELECT ip, hostname FROM %s WHERE ip=%%s" %
				(constants.tblSeenMachines),
			ipInt )
		results = c.fetchall()

		if len(results) > 0:
			# We found a corresponding entry, check the name in the entry
			# matches
			name = results[0][1]
			if machine.name and name != machine.name:
				# Name is different, update name change in the database
				log.info( "Machine name %s is different to stored name %s! "\
					"Updating...", machine.name, name )
				c.execute( "UPDATE %s SET hostname=%%s WHERE ip=%%s" % \
					(constants.tblSeenMachines), (machine.name, ipInt) )
			else:
				log.info( "Retrieved machine %s (%s) from database",
					name, machine.ip )
		else:
			log.info( "Adding machine %s (%s)", machine.name, machine.ip )
			c.execute( "INSERT INTO %s (ip, hostname) VALUES (%%s, %%s)" % \
					(constants.tblSeenMachines),
				(ipInt, machine.name) )
		c.close()

	def logNewProcess( self, process ):
		ipInt = utilities.ipToInt( process.machine.ip )

		try:
			procPref = self.getProcessPref( process )
		except KeyError:
			# this process is not in our preferences, ignore quietly
			return

		c = self.dbManager.getLogDbCursor()
		c.execute("SELECT id FROM %s "
					"WHERE machine = %%s AND pid = %%s AND name = %%s" \
				% (constants.tblSeenProcesses),
			(ipInt, process.pid, process.label()) )
		results = c.fetchall()

		if len(results) > 0:
			# Great! We already found our process then
			dbId = results[0][0]
			try:
				userName = self.cluster.getUser( process.uid ).name
			except cluster.User.error:
				userName = "<%d>" % process.uid

			log.info( "Retrieved process %s (user:%s, pid:%d, host:%s) from " \
				"database (dbid %d)", process.label(), userName,
				process.pid, process.machine.name, dbId )
		else:
			c.execute( "INSERT INTO %s "\
						"(machine, userid, name, processPref, pid) "\
						"VALUES (%%s, %%s, %%s, %%s, %%s)" \
					% (constants.tblSeenProcesses),
				(ipInt, process.uid, process.label(), procPref.dbId,
					process.pid)
			)
			dbId = c.connection.insert_id()

			try:
				userName = self.cluster.getUser( process.uid ).name
			except cluster.User.error:
				userName = "<%d>" % process.uid

			log.info( "Added process %s (user:%s, pid:%d, host:%s)"\
					" to database. (dbid %d)",
				process.label(), userName, process.pid,
				process.machine.name, dbId )

		self.dbIds[ process ] = dbId
		c.close()

	def logNewUser(self, user):
		c = self.dbManager.getLogDbCursor()
		if hasattr(user, "uid"):
			c.execute("SELECT name FROM %s "
				"WHERE uid = %%s" % (constants.tblSeenUsers), user.uid)
		else:
			log.warning("User object %%s has no attribute uid" % (user))
			return

		# If the user hasn't been logged to the database, do it now
		if c.rowcount == 0:
			c.execute("INSERT INTO %s (uid, name)" \
					"VALUES (%%s, %%s)" %
					(constants.tblSeenUsers), \
					(user.uid, user.name) )
		c.close()


	def logAll(self):
		for m in self.machineStats.iterkeys():
			self.logMachineStats( m )

		for p in self.processStats.iterkeys():
			self.logProcessStats( p )

	def logMachineStats( self, machine ):
		sql = self.machineLogSql

		stats = self.machineStats[ machine ]
		ipInt = utilities.ipToInt( machine.ip )

		statList = []
		for i in range( len( stats.rows ) ):
			statRow = stats.rows.pop(0)
			values = [ statRow.tick, ipInt ]
			values.extend( statRow.values )
			statList.append( values )

		self.statDumper.push( sql, statList )
		#print "SQL: %s Statlist: %s" % (sql, statList)

	def logProcessStats( self, process ):
		procPref = self.getProcessPref( process )
		stats = self.processStats[ process ]
		sql = self.processLogSql[ procPref.name ]
		dbId = self.dbIds[ process ]

		statList = []
		for i in range( len( stats.rows ) ):
			statRow = stats.rows.pop(0)
			values = [ statRow.tick, dbId ]
			values.extend( statRow.values )
			statList.append( values )

		self.statDumper.push( sql, statList )
		#print "SQL: %s Statlist: %s" % (sql, statList)


	def consolidateAll( self, shouldLimitDeletion=True ):
		"""
		Consolidate statistics in all aggregation windows.
		"""

		for windowPref in self.windowPrefs:
			windowId = windowPref.dbId
			samples = windowPref.samples
			samplePeriodTicks = windowPref.samplePeriodTicks
			aggregateWindow = self.tick - samplePeriodTicks
			threshold = int(self.tick - (samples * samplePeriodTicks))


			# Consolidate from the window below into the current one
			if windowId > 1 and self.tick % samplePeriodTicks == 0 \
				and shouldLimitDeletion:

				#print "consolidating machine statistics for window %d: "\
				#	"%d samples @ %d ticks per sample (current tick %d)" % \
				#	(windowId, samples, samplePeriodTicks, self.tick)

				windowTableName = \
					"%s_lvl_%03d" % (constants.tblStatMachines, windowId)
				lastWindowTableName = \
					"%s_lvl_%03d" % (constants.tblStatMachines, windowId - 1)

				#print self.machineConsolidateSql % (
				#	windowTableName, self.tick,
				#	lastWindowTableName, self.tick - samplePeriodTicks,
				#	self.tick)

				self.statDumper.push( self.machineConsolidateSql % (
						windowTableName, self.tick,
						lastWindowTableName, aggregateWindow, self.tick) )

				#print self.machineConsolidateSql % \
				#	(windowTableName, lastWindowTableName, "%s")

				for procPref in self.prefTree.iterProcPrefs():
					#print "consolidating process prefs for %s for window %d "\
					#		"(interval %d)" %\
					#	(procPref.name, windowId, samplePeriodTicks)
					windowTableName = \
						"%s_lvl_%03d" % (procPref.tableName, windowId)
					lastWindowTableName = \
						"%s_lvl_%03d" % (procPref.tableName, windowId - 1)

					consolidateSql =  self.processConsolidateSql[procPref] % \
							(windowTableName, self.tick,
							lastWindowTableName, aggregateWindow, self.tick)

					#print consolidateSql
					self.statDumper.push( consolidateSql )

			# remove extra rows from process statistics
			if threshold > 0:
				#log.debug( "Clearing values on level %03d earlier than %d",
				#	windowId, threshold )
				for procPref in self.prefTree.iterProcPrefs():
					tableName = "%s_lvl_%03d" % (procPref.tableName, windowId)
					sql = \
						"""
						DELETE FROM %s
						WHERE %s.tick < %s
						""" \
						% (tableName, tableName, threshold)

					if shouldLimitDeletion:
						sql += (" LIMIT %s" % self.statDumper.deleteSize)

					self.statDumper.push( sql )

				# remove extra rows from machine statistics
				tableName = "%s_lvl_%03d" % \
						(constants.tblStatMachines, windowId)
				sql = \
					"""
					DELETE FROM %s
					WHERE %s.tick < %s
					""" \
					% (tableName, tableName, threshold)

				if shouldLimitDeletion:
					sql += (" LIMIT %s" % self.statDumper.deleteSize)

				self.statDumper.push( sql )

				# remove extra unused ticks no longer referenced by any
				# window
				if windowPref == self.windowPrefs[-1]:
					sql = \
						"""
						DELETE FROM %s WHERE id < %s
						""" \
						 % (constants.tblStdTickTimes, threshold) 

					if shouldLimitDeletion:
						sql += (" LIMIT %s" % self.statDumper.deleteSize)

					self.statDumper.push( sql )
			#else:
			#	log.debug("Not clearing values on level %03d earlier than %d",
			#		windowId, threshold )


	def getProcessPref( self, p ):
		if (self.processNameToPref.has_key( p.name )):
			return self.prefTree.procPrefs[ self.processNameToPref[ p.name ] ]

		for procPref in self.prefTree.iterProcPrefs():
			#regexp = re.compile(procPref.matchtext)
			#if regexp.match( p.name ):
			if procPref.matchtext == p.name:
				self.processNameToPref[ p.name ] = procPref.name
				return procPref

		raise KeyError( "No process pref for expected process: %s" % (p) )

	def checkDeadProcessesAndMachines( self ):
		now = time.time()

		currentMachines = set(self.cluster.getMachines())
		currentProcesses = set(self.cluster.getProcs())

		machinesToDelete = set()
		processesToDelete = set()

		for machine in self.machineStats.iterkeys():
			if not machine in currentMachines:
				log.info( "Lost machine %s (%s)", machine.name, machine.ip )
				machinesToDelete.add( machine )

				# Note: Usually the processes in machine have already been
				#		removed from the procs member, but this is just a
				#       precaution.
				for process in machine.procs.itervalues():
					try:
						user = self.cluster.getUser( process.uid ).name
					except cluster.User.error, e:
						log.warning(e)
						user = process.uid
					log.info( "...and thus we lost process %s " \
						"(user:%s, pid:%d, host:%s)",
						process.label(), user, process.pid,
						process.machine.name )
					del self.processStats[ process ]
					processesToDelete.add( process )

		for process in self.processStats.iterkeys():
			if not process in currentProcesses and \
			not process in processesToDelete:
				try:
					user = self.cluster.getUser( process.uid ).name
				except cluster.User.error, e:
					log.warning(e)
					user = process.uid
				log.info( "Lost process %s(user:%s, pid:%d, host:%s)",
					process.label(), user, process.pid,
					process.machine.name )
				processesToDelete.add( process )

		for m in machinesToDelete:
			del self.machineStats[m]

		for p in processesToDelete:
			del self.processStats[p]
			del self.dbIds[p]

	def finishUp( self ):
		log.info( "Stopping data collection (Current tick is %i)." \
			% self.tick )

		self.finishLogLock.acquire()
		self.finishLog = True
		self.finishEvent = threading.Event()
		self.finishLogLock.release()
		return self.finishEvent


class StatDumper( threading.Thread ):
	"""
	Statistics dumping thread.
	"""

	def __init__( self, dbManager ):
		""" Constructor. """
		threading.Thread.__init__( self, name="StatDumper" )

		# a queue of ( sql, [(value, ...), (value, ...), ...] )
		self._queue = Queue.Queue()
		self.dbManager = dbManager
		self.finishDump = False
		self.finishDumpEvent = None
		self.currentQuery = None
		self.currentQueryStart = None
		self.listenForDebugger = False
		self.exception = None
		self.deleteSize = 20

	def run( self ):
		try:
			self.doRun()
		except Exception, e:
			self.exception = (sys.exc_type, sys.exc_value, sys.exc_traceback)
			# the thread will die from here - the main thread should call
			# raiseDeferredException() below to re-raise this exception in the
			# main thread.


	def raiseDeferredException( self ):
		if self.exception:
			log.error( traceback.format_exception( *self.exception ) )
			raise self.exception[1]


	def doRun( self ):
		cursor = self.dbManager.getLogDbCursor()

		while not self._queue.empty() or not self.finishDump:
			try:
				# Block for 0.5 seconds before going round the loop again
				# (To check if finishDump has changed)
				sql, valuesList = self._queue.get( True, 1.0 )
				#sql, valuesList = self._queue.get( )
				if valuesList == None:
					self.currentQueryStart = time.time()
					self.currentQuery = sql
					count = cursor.execute( sql )
					queryLength = time.time() - self.currentQueryStart
					if queryLength > 3:
						log.warning("Query took %f seconds: %s",
							queryLength, sql )
					self.currentQueryStart = None
				else:
					for values in valuesList:
						#print "SQL(values):%s\n %s!" % (str(values), sql,)
						self.currentQueryStart = time.time()
						self.currentQuery = sql
						count = cursor.execute( sql, values )
						queryLength = time.time() - self.currentQueryStart
						if queryLength > 3:
							log.warning( "Query took %f seconds: %s "\
									"(values: %s)",
								queryLength, sql, values )
						self.currentQueryStart = None
			except Queue.Empty:
				pass

		log.info( "StatDumper: Finished dumping to sql!" )
		self.finishDumpEvent.set()
		return

	def getCurrentQueryInfo( self ):
		# sample the current query start
		currentQueryStart = self.currentQueryStart
		if currentQueryStart != None:
			queryDuration = time.time() - currentQueryStart
			sql = self.currentQuery
			return (sql, queryDuration)
		else:
			return None

	def push( self, sql, values=None ):
		"""
		Push an SQL statement and the list of values for the SQL into the queue
		to be dumped to the database. The SQL statement is executed however
		once for each tuple element in the list of values.

		@param sql: 	the SQL template, that is interpolated by the database
						DBI driver
		@param values:	a list of tuples, each tuple containing values for the
						SQL template
		"""
		self._queue.put_nowait( (sql, values) )

	def queueSize( self ):
		return self._queue.qsize()

	def finishUp( self):
		self.finishDumpEvent = threading.Event()
		self.finishDump = True
		log.debug("Begin finishing up dump thread")
		return self.finishDumpEvent

# gather.py
