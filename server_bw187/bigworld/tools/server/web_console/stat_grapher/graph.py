# Import standard python libraries
import time
import sys
import os
import traceback
import logging
import random
import socket
import struct
from cStringIO import StringIO

# Import third party libraries
import turbogears
import turbogears.config
from turbogears import expose
import cherrypy
import amfgateway
from flashticle.amf import to_amf, typedobject
from itertools import chain, izip
import sqlobject
from sqlobject.sqlbuilder import *

# Import local modules
import managedb
import model

# Import stuff from stat_logger
import bwsetup
bwsetup.addPath("../../stat_logger")
bwsetup.addPath("../..")
from pycommon import cluster
import constants
import prefclasses
import prefxml

# Import common SQLObject classes
from web_console.common import util
from web_console.common import model as commonModel


# Logger objects
sqlLog = logging.getLogger( "stat_grapher.sql" )
apiLog = logging.getLogger( "stat_grapher.api" )
amfLog = logging.getLogger( "stat_grapher.amf" )

def inet_ntoa( ipNum ):
	"""Return the text of the given integer IP address."""
	return socket.inet_ntoa( struct.pack( '@I', ipNum ) )

def inet_aton( ipString ):
	"""
	Return integer IP address given its string representation as a dotted
	quad.
	"""
	return struct.unpack( '@I', socket.inet_aton( ipString ) )[0]


class ProcessInfo:
	def __init__( self, dbId, pType, pName, pPid, mName ):
		self.dbId = dbId
		self.pType = pType
		self.name = pName
		self.pid = pPid
		self.machine = mName

		if pType.lower() in ("cellapp", "baseapp"):
			self.id = pName[len(pType):]
		else:
			self.id = None

	def __str__( self ):
		return "Process %s(%d) on %s" % (self.name, self.pid, self.machine)

class MachineInfo:
	def __init__( self, ip, hostname ):
		self.ip = ip
		self.hostname = hostname

	def ipAsString( self ):
		return inet_ntoa( self.ip )

	def __str__( self ):
		return "Machine %s (%s)" % \
			(self.hostname, self.ipAsString())

	def __repr__( self ):
		return str( self )

class UserDisplayPrefs:
	"""
	In-memory copy of the user display preferences array (specific to a user
	and a log database) from the StatGrapherPrefs sqlobject-based database
	table. Retrieved on init time from StatLogger's preference file, and
	updates to this object propagate to the database table.
	"""

	def __init__( self, logDb, uid=None ):
		"""
		Constructor. Passing None for the uid retrieves the default preferences.

		@param logDb:	the log database
		@param uid:		the user ID
		"""
		apiLog.info( "UserDisplayPrefs.__init__: %r, %r", logDb, uid )
		self._displayPrefs = None
		if uid:
			self._uid = uid
			self._isDefault = False
		else:
			self._uid = None
			self._isDefault = True

		self._logDb = str( logDb )
		self.refreshFromDB()


	def dict( self ):
		"""
		Return the dictionary containing the user display preferences.
		Modifications to this dictionary can be updated to the database through
		the updateToDB method.
		"""
		return self._displayPrefs


	def updateToDB( self ):
		"""
		Save the preferences to the database.
		"""
		if self._isDefault:
			return

		rows = model.StatGrapherPrefs.selectBy(
			uid = self._uid,
			log = self._logDb )
		record = rows[0]
		record.displayPrefs = self._displayPrefs


	def refreshFromDB( self ):
		"""
		Refreshes the dictionary taking values from the DB.
		"""
		if self._isDefault:
			self._displayPrefs = self._generateDefaultDisplayPrefs()
			return

		rows = model.StatGrapherPrefs.selectBy(
			uid = self._uid,
			log = self._logDb )
		if rows.count() == 1:
			record = rows[0]
		else:
			displayPrefs = {
				"enabledProcStatOrder" 		: {},
				"procPrefs"					: {},
				"enabledMachineStatOrder"	: None,
				"machineStatPrefs"			: {}
			}
			record = model.StatGrapherPrefs(
				uid = self._uid,
				log = self._logDb,
				displayPrefs = displayPrefs )

		self._displayPrefs = record.displayPrefs


	def _generateDefaultDisplayPrefs( self ):

		# We retrieve the pref tree twice, once from the database, which
		# contains the database ID for each named stat, and once from the XML.

		# Only the XML file contains "colour" and "visible" values, as they 
		# aren't stored in the database. If a stat exists in the database
		# but doesn't exist in the XML, then we need to generate a random
		# default colour.

		# stat_logger preferences.xml file located in the stat_logger
		# directory, which contains the actual default display preferences.


		options, xmlPrefTree = prefxml.loadPrefsFromXMLFile(
			turbogears.config.get( "stat_logger.preferences",
				"../stat_logger/preferences.xml" ) )
		dbManager, dbPrefTree = managedb.ptManager.requestDbManager(
			self._logDb )


		# build display prefs dictionary
		displayPrefs = {
			# order of the visible process stats, by process type
			"enabledProcStatOrder"		: {},
			# process stat-specific preferences
			# e.g. colour
 			"procPrefs"					: {},
			# order of the visible machine stats
			"enabledMachineStatOrder"	: [],
			# machine stat-specific preferences
			# e.g. colour
			"machineStatPrefs"			: {}
		}

		enabledProcStatOrder = displayPrefs['enabledProcStatOrder']
		enabledMachineStatOrder = displayPrefs['enabledMachineStatOrder']

		#apiLog.info( "*** xmlPrefTree: %s", xmlPrefTree )
		#apiLog.info( "*** dbPrefTree: %s", dbPrefTree )

		for dbProcPref in dbPrefTree.iterProcPrefs():
			# Get process preferences from xml file
			xmlProcPref = xmlPrefTree.procPrefByName( dbProcPref.name )
			# Initialiase stat order list for that process
			enabledProcStatOrder[dbProcPref.name] = []
			# Initialise dictionary for that process
			displayPrefs[ "procPrefs" ][ dbProcPref.name ] = {}
			# Copy XML stat pref colour and show values to our
			# display prefs.
			for dbProcStatPref in dbProcPref.iterAllStatPrefs():
				statPrefId = str( dbProcStatPref.dbId )

				displayStatPref = {}
				displayPrefs["procPrefs"][dbProcPref.name][statPrefId] = \
					displayStatPref
				try:
					xmlProcStatPref = xmlProcPref.statPrefByName(
						dbProcStatPref.name )
					statColour = xmlProcStatPref.colour
					statShow = xmlProcStatPref.show
				except KeyError:
					statColour = prefxml._randomColour()
					statShow = False

				displayStatPref["colour"] = "%06X" % statColour

				if statShow:
					enabledProcStatOrder[dbProcPref.name].append( statPrefId )

		for dbMachineStatPref in dbPrefTree.iterMachineStatPrefs():

			statPrefId = str( dbMachineStatPref.dbId )

			displayStatPref = {}
			displayPrefs["machineStatPrefs"][statPrefId] = displayStatPref

			try:
				xmlMachineStatPref = xmlPrefTree.machineStatPrefByName(
					dbMachineStatPref.name )
				statColour = xmlMachineStatPref.colour
				statShow = xmlMachineStatPref.show
			except KeyError:
				statColour = prefxml._randomColour()
				statShow = False

			displayStatPref["colour"] = "%06X" % statColour

			if statShow:
				enabledMachineStatOrder.append( statPrefId )

		#apiLog.info( "UserDisplayPrefs._generateDisplayPrefs: returning %r", displayPrefs )

		return displayPrefs


# -----------------------------------------------------------------------------
# Section: AMF conversion functions
# -----------------------------------------------------------------------------

@to_amf.when("isinstance(obj, ProcessInfo)")
def to_amf_processinfo( pi ):
	return to_amf(dict (
		dbId = pi.dbId,
		type =  pi.pType,
		name = pi.name,
		pid = pi.pid,
		id = pi.id,
		machine = pi.machine,
	) )


@to_amf.when("isinstance(obj, MachineInfo)")
def to_amf_machineinfo( mi ):
	return to_amf(dict (
		ip = mi.ipAsString(),
		hostname = mi.hostname
	) )


@to_amf.when("isinstance(obj, prefclasses.PrefTree)")
def to_amf_preftree( prefTree ):
	try:
		ptDict = {}
		ptDict["machineStatPrefs"] = {}
		for msPref in prefTree.iterMachineStatPrefs():
			ptDict[ "machineStatPrefs" ][ str(msPref.dbId) ] = msPref

		ptDict["procPrefs"] = {}
		for procPref in prefTree.iterProcPrefs():
			ptProcPref = {}
			ptProcPref["name"] = procPref.name
			ptProcPref["dbId"] = procPref.dbId
			ptProcPref["statPrefs"] = {}
			for i in procPref.iterAllStatPrefs():
				ptProcPref["statPrefs"][ str(i.dbId) ] = i
			ptDict["procPrefs"][procPref.name] = ptProcPref

		ptDict["windowPrefs"] = prefTree.windowPrefs
		ptDict["tickInterval"] = prefTree.tickInterval

		#amfLog.debug( "%r", ptDict )

		return to_amf( ptDict )
	except Exception, e:
		amfLog.error( traceback.format_exc() )
		amfLog.error( "to_amf_preftree: %s: %s", e.__class__.__name__, e )
		raise e


@to_amf.when("isinstance(obj, prefclasses.StatPref)")
def to_amf_statPref( obj ):
	try:
		spDict = {}
		spDict["name"] = obj.name
		spDict["dbId"] = obj.dbId
		spDict["valueAt"] = obj.valueAt
		spDict["maxAt"] = obj.maxAt
		spDict["type"] = obj.type
		return to_amf( spDict )
	except Exception, e:
		amfLog.error( traceback.format_exc() )
		amfLog.error( "to_amf_statPref: %s: %s", e.__class__.__name__, e )
		raise e


# Might be good to send the window prefs over, even
# if we aren't going to use it. It's a once off cost,
# so it won't make a difference.
@to_amf.when("isinstance(obj, prefclasses.WindowPref)")
def to_amf_windowPref( obj ):
	wpDict = {}
	wpDict["samples"] = obj.samples
	wpDict["samplePeriodTicks"] = obj.samplePeriodTicks
	return to_amf( wpDict )


# -----------------------------------------------------------------------------
# Section: StatGrapherBackend controller class
# -----------------------------------------------------------------------------
class StatGrapherBackend( amfgateway.AMFGateway ):
	"""
	Controller for StatGrapher, and also serves as its backend.
	"""

	def __init__( self ):
		"""
		Constructor.
		"""

		amfgateway.AMFGateway.__init__( self, "StatGrapherBackend" )
		self.cluster = cluster.Cluster()
		self.userDisplayPrefs = {}
		self._pageSpecificJS = [
			"/statg/static/js/exception.js",
			"/statg/static/js/flashtag.js",
			"/statg/static/js/flashserializer.js",
			"/statg/static/js/flashproxy.js",
			"/statg/static/js/vbcallback.vbs"
		]

	# -------------------------------------------------------------------------
	# Section: Exposed TurboGears Methods
	# -------------------------------------------------------------------------
	@expose( template="stat_grapher.templates.graphview" )
	def index( self, logDb, user, desiredPointsPerView=80,
			minGraphWidth=250, minGraphHeight=200, debug=False ):
		"""
		Make the process view for a particular user and log database.
		"""
		return dict(
			log = logDb,
			user = user,
			serviceURL = cherrypy.request.base + "/statg/graph/amfgateway",
			profile = "process",
			desiredPointsPerView = desiredPointsPerView,
			page_specific_js = self._pageSpecificJS,
			minGraphWidth = minGraphWidth,
			minGraphHeight = minGraphHeight,
			debug = debug
		)


	@expose( template="stat_grapher.templates.graphview" )
	def machines( self, logDb, desiredPointsPerView=80,
			minGraphWidth=250, minGraphHeight=200, debug=False ):
		"""
		Make the machine view for a particular log database.
		"""
		apiLog.info( "machines, logDb = %s", logDb )
		return dict(
			log = logDb,
			user = None,
			serviceURL = cherrypy.request.base + "/statg/graph/amfgateway",
			profile = "machine",
			desiredPointsPerView = desiredPointsPerView,
			minGraphWidth = minGraphWidth,
			minGraphHeight = minGraphHeight,
			debug = debug
		)

	# -------------------------------------------------------------------------
	# Section: Exposed AMF Gateway Methods
	# -------------------------------------------------------------------------
	@amfgateway.expose
	def requestSpecificProcessInfo( self, logDb, user, processType, processIds,
			startTime, timeRange ):

		return self.requestProcessInfo( logDb, user, processType, startTime,
			endTime, processIds )


	@amfgateway.expose
	def requestLogRange( self, logDb ):
		funcStart = time.time()
		#apiLog.info( "requestLogRange: logDb = %s", logDb )

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		cursor = dbManager.getLogDbCursor()
		#cursor.execute( """
		#	SELECT MIN( id ), MAX( id ) FROM %s"""
		#		% (constants.tblStdTickTimes) )
		#startTime, endTime = cursor.fetchone()

		query = """
			SELECT id, time FROM %s
			ORDER BY id ASC LIMIT 1
			""" % (constants.tblStdTickTimes)

		queryStart = time.time()
		sqlLog.debug( "requestLogRange START query: %s", query)
		cursor.execute( query )
		sqlLog.debug( " ...query took %fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )

		startTick, startTime = cursor.fetchone()

		# Grab the second last tick, as the stats might not have completed
		# being collected for the very last tick (ticks are written before
		# stats are collected
		query = """
			SELECT id, time FROM %s
			ORDER BY id DESC LIMIT 1, 1
			""" % (constants.tblStdTickTimes)
		queryStart = time.time()
		sqlLog.debug( "requestLogRange END query: %s", query )
		cursor.execute( query )
		sqlLog.debug( " ...query took %fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )

		endTick, endTime = cursor.fetchone()

		# TEST DEBUG QUERY
		query = """
			SELECT id, time FROM %s
			ORDER BY id DESC LIMIT 1
			""" % (constants.tblStdTickTimes)
		cursor.execute( query )
		debugEndTick, debugEndTime = cursor.fetchone()

		cursor.close()

		apiLog.info( "requestLogRange: End tick difference is %f",
			debugEndTime - endTime )
		apiLog.info( "requestLogRange: Retrieved ticks %d - %d (%f - %f)",
			startTick, endTick, startTime, endTime )
		apiLog.info( "requestLogRange: Returning %f - %f. Took %.3fs",
			startTime, endTime, time.time() - funcStart )
		return {"start": startTime, "end": endTime}


	@amfgateway.expose
	def requestDisplayPrefs( self, logDb, user=None ):
		#apiLog.debug( "requestDisplayPrefs" )
		sessionUID = util.getSessionUID()

		# Retrieve or generate default display preferences as a basis
		defaultPrefs = self.getUserDisplayPrefs( logDb )
		outDict = defaultPrefs.dict()

		userPrefs = self.getUserDisplayPrefs( logDb, uid = sessionUID )
		userDict = userPrefs.dict()

		# Now combine default and user preferences
		for procName, procPref in userDict["procPrefs"].iteritems():
			for statId, valueDict in procPref.iteritems():
				outDict["procPrefs"][procName][str( statId )].update(
					valueDict )

		for procType, enabledStats in \
				userDict[ "enabledProcStatOrder" ].iteritems():
			outDict["enabledProcStatOrder"][procType] = enabledStats

		for statId, valueDict in userDict[ "machineStatPrefs" ].iteritems():
			outDict["machineStatPrefs"][str( statId )].update( valueDict )

		if userDict[ "enabledMachineStatOrder" ]:
			outDict["enabledMachineStatOrder"] = \
				userDict[ "enabledMachineStatOrder" ]

		#apiLog.debug( "requestDisplayPrefs: returning %r", outDict )

		return outDict


	@amfgateway.expose
	def requestPrefTree( self, logDb ):
		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		tickInterval = dbManager.getLogDbInterval()
		prefTree.tickInterval = tickInterval

		#apiLog.debug( "requestPrefTree" )
		#print to_amf(prefTree)
		return prefTree


	@amfgateway.expose
	def setMachineStatColour( self, logDb, prefId, newColour ):
		sessionUID = util.getSessionUID()

		userPrefs = self.getUserDisplayPrefs( logDb, uid = sessionUID )

		# Construct the tree branches if they don't exist
		machinePrefsDict = userPrefs.dict().setdefault( "machineStatPrefs", {} )
		prefIdDict = machinePrefsDict.setdefault( prefId, {} )
		if newColour == None:
			try:
				del prefIdDict[ "colour" ]
			except:
				pass
		else:
			prefIdDict[ "colour" ] = newColour

		userPrefs.updateToDB()

		apiLog.debug( "setMachineStatColour: colour has been set" )


	@amfgateway.expose
	def setProcessStatColour( self, logDb, procType, prefId, newColour ):
		sessionUID = util.getSessionUID()

		userPrefs = self.getUserDisplayPrefs( logDb, uid = sessionUID )

		# Construct the tree branches if they don't exist
		procPrefsDict = userPrefs.dict().setdefault( "procPrefs", {} )
		procTypeDict = procPrefsDict.setdefault( procType, {} )
		prefIdDict = procTypeDict.setdefault( prefId, {} )
		if newColour == None:
			try:
				del prefIdDict[ "colour" ]
			except:
				pass
		else:
			prefIdDict[ "colour" ] = newColour

		userPrefs.updateToDB()

		apiLog.debug( "setProcessStatColour: colour has been set" )


	@amfgateway.expose
	def saveEnabledProcStatOrder( self, logDb, procType, statList ):
		"""
		Save the process statistics order list for the given log and process
		type.

		@param logDb		the log database
		@param procType		the process type
		@param statList 	list of ordered stat ids in unicode string form
		"""

		apiLog.debug( "saveEnabledProcStatOrder: "
				"logDb=%s, procType=%s, statList=%s",
			logDb, procType, statList )
		sessionUID = util.getSessionUID()

		userPrefs = self.getUserDisplayPrefs( logDb, uid = sessionUID )
		userDict = userPrefs.dict()

		# Construct the tree branches if they don't exist
		enabledProcStatOrder = userDict.setdefault(
			"enabledProcStatOrder", {} )

		enabledProcStatOrder[procType] = statList

		userPrefs.updateToDB()


	@amfgateway.expose
	def saveEnabledMachineStatOrder( self, logDb, statList ):
		"""
		Save the machine statistics order list for the given log.

		@param logDb		the log database
		@param statList		list of ordered statistic IDs in unicode string form
		"""
		apiLog.debug( "saveEnabledMachineStatOrder: "
			"logDb = %s, statList = %s", logDb, statList )
		sessionUID = util.getSessionUID()

		userPrefs = self.getUserDisplayPrefs( logDb, uid = sessionUID )
		userDict = userPrefs.dict()

		userDict["enabledMachineStatOrder"] = statList

		userPrefs.updateToDB()


	@amfgateway.expose
	def requestSkeletonData( self, logDb, uid, procType, dbIDs ):
		"""
		Get all available skeleton process statistics.
		"""
		apiLog.info( "requestSkeletonData: "
				"logDb = %s, uid = %s, procType = %s, dbIds = %s",
			logDb, uid, procType, dbIDs )
		sessionUID = util.getSessionUID()

		rows = self.getSkeletonData( logDb, uid, sessionUID,
			procType, dbIDs, 0 )
		return rows


	@amfgateway.expose
	def updateSkeletonData( self, logDb, uid, procType, dbIDs, sinceTick ):
		"""
		Get the available skeleton process statistics from a certain tick.
		"""
		apiLog.info( "updateSkeletonData: "
				"logDb = %s, uid = %s, procType = %s, dbIDs = %s, "
				"sinceTick = %s",
			logDb, uid, procType, sinceTick )
		sessionUID = util.getSessionUID()

		rows = self.getSkeletonData( logDb, uid, sessionUID, procType, dbIDs,
			int( sinceTick ) )
		return rows


	@amfgateway.expose
	def requestProcessInfo( self, logDb, user, processType, startTime,
			endTime, dbIds = None ):
		"""
		Request details about processes for a certain user.
		"""

		apiLog.info( "requestProcessInfo: logDb = %s, user = %s, "
				"processType = %s, startTime = %s, endTime = %s, dbIds = %s ",
			logDb, user, processType, startTime, endTime, dbIds )


		# Just a few initialisations
		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		procPref = prefTree.procPrefs[processType]
		cursor = dbManager.getLogDbCursor()

		# Step 1: Get some variables from the database
		fromTick, toTick = self.getTickRange( dbManager, startTime, endTime )
		endTick, endTime = dbManager.getLastTick()
		tickInterval = dbManager.getLogDbInterval()

		# Step 2: Determine which resolution will cover the time range that we
		# want
		#wantedWindow = prefTree.windowPrefs[0]
		#for window in prefTree.windowPrefs:
		#	windowTickStart = endTick - \
		#		window.samples * window.samplePeriodTicks
		#	if startTick > windowTickStart:
		#		wantedWindow = window
		#		break
		for window in prefTree.windowPrefs:
			apiLog.info( "requestProcessInfo: Looping through %d", window.dbId )
			windowStart = (endTime - \
				(window.samples * window.samplePeriodTicks) * \
				tickInterval) * 1000
			apiLog.info( "requestProcessInfo: window %d windowStart is %s (our start %s)",
				window.dbId, windowStart, startTime )

			if windowStart < startTime:
				apiLog.info( "requestProcessInfo: This window is good, " \
					"we can use this" )
				wantedWindow = window
				break

		# Step 3: Get the new processes
		tableName = "%s_lvl_%03d" % (procPref.tableName, wantedWindow.dbId)
		query = "SELECT DISTINCT(process) FROM %s s "\
				"JOIN %s p ON s.process = p.id " \
				"WHERE p.userid = %s " % \
				( tableName, constants.tblSeenProcesses, user )

		if fromTick:
			query += "AND tick > %s " % (fromTick)

		if toTick:
			query += "AND tick < %s " % (toTick)

		if dbIds != None and len( dbIds ) > 0:
			query += " AND s.process IN (%s)" % \
				(",".join(str(int(p)) for p in dbIds))

		queryStart = time.time()
		sqlLog.debug( "requestProcessInfo query: %s", query )
		cursor.execute( query )
		sqlLog.debug( " ...query took %fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )
		results = cursor.fetchall()

		procIds = [r[0] for r in results]
		procList = self.getProcessDetails( procIds, cursor )
		cursor.close()

		#return dict( procs = procList )

		apiLog.debug("Process list: %s (Type is: %s)",
			[proc.name for proc in procList], type( procList ) )
		return procList


	@amfgateway.expose
	def requestData( self, logDb, uid, procType, dbIDs,
			fromTick, toTick, resolution ):
		"""
		Retrieve the higher-resolution process statistic data.
		"""
		funcStart = time.time()

		apiLog.info( "requestData: logDb = %s, uid = %s, procType = %s, "
				"dbIDs = %s, fromTick = %s, toTick = %s, resolution = %s",
			logDb, uid, procType, dbIDs, fromTick, toTick, resolution )

		sessionUID = util.getSessionUID()
		if not sessionUID:
			sessionUID = 2

		# TODO: treat the resolution parameter as the actual aggregation window
		# for now. Later, we would want to select based on some other metric
		# so the client would not need to know about actual aggregation windows
		# (however, this isn't too much information to pass over to client..
		# consider doing this too! )
		apiLog.debug( "requestData Pre-Preftree Checkpoint: Taken %.3fs so far",
			time.time() - funcStart )

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		window = prefTree.windowPrefs[ int( resolution ) ]

		apiLog.debug( "requestData Pre-SQL Checkpoint: Taken %.3fs so far",
			time.time() - funcStart )

		data = self.getProcessStatisticsData( logDb, uid, sessionUID, procType,
			dbIDs, window, fromTick, toTick )
		#apiLog.debug(data)

		apiLog.debug( "requestData: Took %.3fs", time.time() - funcStart )

		return data

	requestProcessStatistics = requestData

	@amfgateway.expose
	def requestDataByTime( self, logDb, uid, procType, dbIDs,
			fromTime, toTime, resolution ):
		"""
		Retrieve the higher-resolution process statistic data.
		Uses timestamps as the start and end markers.
		"""
		funcStart = time.time()
		apiLog.info( "requestDataByTime: logDb = %s, uid = %s, fromTime = %s, "
				"toTime = %r(%s), resolution = %s",
			logDb, uid, fromTime, toTime, type( toTime ), resolution )

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )

		tickRangeStart = time.time()
		fromTick, toTick = self.getTickRange( dbManager, fromTime, toTime )
		apiLog.info( "requestDataByTime tickRange checkpoint: "
				"Get tick range took %.3fs",
			time.time() - tickRangeStart )


		result = self.requestData( logDb, uid, procType, dbIDs,
			fromTick, toTick, resolution )

		apiLog.info( "requestDatabyTime: Took %.3fs", time.time() - funcStart )

		return result

	requestProcessStatisticsByTime = requestDataByTime

	@amfgateway.expose
	def requestMachineInfo( self, logDb, startTime, endTime, ips = None ):
		"""
		Request details about machines.
		"""
		apiLog.info( "requestMachineInfo: logDb = %s, startTime = %s, "
				"endTime = %s, ips = %s",
			logDb, startTime, endTime, ips )

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		cursor = dbManager.getLogDbCursor()

		# Step 1: Get the tick range for the new time range
		fromTick, toTick = self.getTickRange( dbManager, startTime, endTime )
		endTick, endTime = dbManager.getLastTick()
		tickInterval = dbManager.getLogDbInterval()

		# Step 2: Determine which resolution will cover the time range that we
		# want, for now just use the lowest
		for window in prefTree.windowPrefs:
			windowStart = (endTime - \
				(window.samples * window.samplePeriodTicks) * \
				tickInterval) * 1000

			if windowStart < startTime:
				wantedWindow = window
				break

		# Step 3: Get the machines
		query = """SELECT DISTINCT(machine.ip), machine.hostname
					FROM %s_lvl_%03d stat
					JOIN %s machine ON stat.machine = machine.ip """ % \
				(constants.tblStatMachines, wantedWindow.dbId,
					constants.tblSeenMachines)

		if fromTick:
			query += "AND tick > %s " % (fromTick)

		if toTick:
			query += "AND tick < %s " % (toTick)

		if ips != None and len( ips ) > 0:
			query += " AND machine.ip IN (%s) " % \
				(",".join( str( inet_aton( ip ) ) for ip in ips ))
		query += " ORDER BY machine.hostname"

		queryStart = time.time()
		sqlLog.debug( "requestMachineInfo query: %s", query )
		cursor.execute( query )
		sqlLog.debug( " ...query took %fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )
		results = cursor.fetchall()

		machineList = [ MachineInfo( *row ) for row in results ]
		cursor.close()

		apiLog.debug( "requestMachineInfo: Machine list: %s", machineList )
		return machineList


	@amfgateway.expose
	def requestMachineData( self, logDb, ips, fromTick, toTick, resolution ):
		"""
		Retrieve the higher-resolution process statistic data.
		"""
		funcStart = time.time()

		apiLog.info( "requestMachineData: logDb = %s, "
				"ips = %s, fromTick = %s, toTick = %s, resolution = %s",
			logDb, ips, fromTick, toTick, resolution )

		sessionUID = util.getSessionUID()
		if not sessionUID:
			sessionUID = 2

		# TODO: treat the resolution parameter as the actual aggregation window
		# for now. Later, we would want to select based on some other metric
		# so the client would not need to know about actual aggregation windows
		# (however, this isn't too much information to pass over to client..
		# consider doing this too! )
		apiLog.debug( "requestMachineData Pre-Preftree Checkpoint: "
				"Taken %.3fs so far",
			time.time() - funcStart )

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		window = prefTree.windowPrefs[ int( resolution ) ]

		apiLog.debug( "requestMachineData Pre-SQL Checkpoint: "
				"Taken %.3fs so far",
			time.time() - funcStart )

		try:
			data = self.getMachineStatisticsData( logDb, sessionUID, ips,
				window, fromTick, toTick )
		except:
			apiLog.info( traceback.format_exc() )
			raise
		#apiLog.debug(data)

		apiLog.debug( "requestMachineData: Took %.3fs",
			time.time() - funcStart )

		return data

	requestMachineStatistics = requestMachineData

	@amfgateway.expose
	def requestMachineDataByTime( self, logDb, ips, fromTime, toTime,
			resolution ):
		"""
		Retrieve the higher-resolution machine statistic data.
		Uses timestamps as the start and end markers.
		"""
		funcStart = time.time()
		apiLog.info( "requestMachineDataByTime: logDb = %s, ips = %s, "
				"fromTime = %s, toTime = %r(%s), resolution = %s",
			logDb, ips, fromTime, toTime, type( toTime ), resolution )

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )

		tickRangeStart = time.time()
		fromTick, toTick = self.getTickRange( dbManager, fromTime, toTime )
		apiLog.info( "requestDataByTime tickRange checkpoint: "
				"Get tick range took %.3fs",
			time.time() - tickRangeStart )

		result = self.requestMachineData( logDb, ips, fromTick, toTick,
			resolution )

		apiLog.info( "requestDatabyTime: Took %.3fs", time.time() - funcStart )

		return result

	requestMachineStatisticsByTime = requestMachineDataByTime

	# -------------------------------------------------------------------------
	# Section: Helper functions
	# -------------------------------------------------------------------------
	def getUserID( self ):
		"""
		Get the UID of the currently logged in user's servername.
		"""
		return self.cluster.getUser( util.getServerUsername() ).uid


	def getUserDisplayPrefs( self, logDb, uid=None ):
		"""
		Get the user display preferences for a given user and log database.
		"""
		if not self.userDisplayPrefs.has_key( (logDb, uid) ):
			userDisplayPrefs = UserDisplayPrefs( logDb, uid )
			self.userDisplayPrefs[ (logDb, uid) ] = userDisplayPrefs
			return userDisplayPrefs

		return self.userDisplayPrefs[ (logDb, uid ) ]


	def getProcessDetails( self, procDbIds, cursor ):
		"""
		Given a list/set of process dbids, get their process info.
		"""

		if len(procDbIds) == 0:
			return []

		query = "SELECT p.id, pp.name, p.name, p.pid, m.hostname FROM %s p "\
				"JOIN %s m ON p.machine = m.ip "\
				"JOIN %s pp ON p.processPref = pp.id "\
				"WHERE p.id IN (%s) "\
				"ORDER BY p.name" % \
					(constants.tblSeenProcesses, constants.tblSeenMachines,
					constants.tblPrefProcesses, ",".join(map(str, procDbIds)))

		ft = time.time()
		#print query
		cursor.execute( query )
		detailResults = cursor.fetchall()
		#log.verbose( "Query executed in %f seconds (%d rows)",
		#	time.time() - ft, cursor.rowcount)

		processInfo = [ProcessInfo(*row) for row in detailResults]

		return processInfo


	def getMachineStatisticsData( self, logDb, sessionUID, ips,
			window, fromTick, toTick ):
		"""
		Get machine statistic data for a window in a time range for the given
		IPs, or all of them if this is specified as None.
		"""
		funcStart = time.time()
		apiLog.info( "=" * 20 )
		apiLog.info( "  getMachineStatisticsData: logDb = %s, "
				"sessionUID=%s, ips = %s, window = %s, "
				"fromTick = %s, toTick = %s",
			logDb, sessionUID, ips, window, fromTick, toTick)

		# Convert ips back to integers
		#apiLog.debug( "Type of ips is: %s", type(ips ) )
		if ips != None:
			ips = [inet_aton( ip ) for ip in ips]

		apiLog.debug( "ips = %s", ips )

		# get display prefs so that we only get the data for the
		# stats this session's user wants
		userPrefs = self.getUserDisplayPrefs( logDb, sessionUID ).dict()
		if userPrefs['enabledMachineStatOrder'] != None:
			enabledStats = userPrefs['enabledMachineStatOrder']
		else:
			defaultStats = self.getUserDisplayPrefs( logDb,	None ).dict()
			enabledStats = defaultStats['enabledMachineStatOrder']

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		cursor = dbManager.getLogDbCursor()

		getMultipleMachines = (ips == None or len( ips ) > 1)

		# get the column names
		columns = [ 'stat.tick', 'tick.time AS tickTime' ]
		if getMultipleMachines:
			columns.append( 'COUNT( DISTINCT stat.machine ) AS numSummarised' )
		else:
			columns.append( '1 AS numSummarised' )

		apiLog.info( "getMachineStatisticsData: enabledStats = %r",
			enabledStats )
		for enabledStat in enabledStats:
			statPref = prefTree.machineStatPrefById( int( enabledStat ) )
			if getMultipleMachines:
				columns.append(  statPref.consolidateColumn( "stat" ) )
			else:
				columns.append( "stat.%s" % statPref.columnName )

		statTableName = "%s_lvl_%03d" % (constants.tblStatMachines, window.dbId)

		#apiLog.info( "getMachineStatisticsData: uid = %s", uid )
		query = "SELECT %s FROM %s stat " % \
			(", ".join( columns ), statTableName)
		query += "JOIN %s tick ON stat.tick = tick.id " % \
			(constants.tblStdTickTimes,)

		query += "WHERE 1 = 1 "
		if ips:
			query += "AND stat.machine IN (%s) " % \
				(", ".join( map( str, ips ) ),)
		if fromTick and fromTick > 0:
			fromTick -= window.samplePeriodTicks
			query += "AND stat.tick > %d " % (fromTick,)
		if toTick:
			toTick += window.samplePeriodTicks
			query += "AND stat.tick < %d " % (toTick,)

		if getMultipleMachines:
			query += "GROUP BY stat.tick "

		query += "ORDER BY stat.tick"

		sqlLog.debug(
			"getMachineStatisticsData query: \"%s\"", query )
		queryStart = time.time()
		cursor.execute( query )
		sqlLog.debug(
			" ...query took %.3fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )
		rows = cursor.fetchall()
		cursor.close()

		transformStats = time.time()
		apiLog.debug(
			"      Beginning query transform..." )
		stats = self.transformRows( enabledStats, rows )
		apiLog.debug(
			"      Query transform took %.3fs", time.time() - transformStats )
		apiLog.debug(
			"   getMachineStatisticsData finished: Took %.3fs",
				time.time() - funcStart )
		apiLog.info( "=" * 20 )

		return stats


	def getProcessStatisticsData( self, logDb, uid, sessionUID,
			procType, dbIDs,
			window, fromTick, toTick ):
		"""
		Get process statistic data for a process type and at a window in a time
		range.

		TODO: This explanation is deprecated.
		The return value is a list of tuples, each tuple containing the process
		ID, tick ID, and the rest of the statistics in the order of the
		user display preferences' enabled process statistic order.


		@param logDb:		the log database name
		@param uid:			the user ID
		@param sessionUID:	the session UID, for getting view preferences
		@param procType:	the process type
		@param dbIDs:		a list of the process database IDs, or None for all
							processes of the given process type
		@param window:		the window aggregation preference
		@param fromTick:	the start tick. If fromTick is 0 and toTick is None,
							then all available data on this aggregation window
							is retrieved.
		@param toTick:		the end tick. If both fromTick and toTick are both
							None, then empty data is returned.
							Whether to include the tick time as well. This is
							useful in skeleton data.
		"""

		funcStart = time.time()
		apiLog.info( "=" * 20 )
		apiLog.info( "  getProcessStatisticsData: logDb = %s, "
				"uid = %s, sessionUID=%s, procType = %s, dbIDs = %s, "
				"window = %s, fromTick = %s, toTick = %s",
			logDb, uid, sessionUID, procType, dbIDs, window, fromTick, toTick )

		# Convert dbIds back to integers
		#apiLog.debug( "Type of dbIDs is: %s", type(dbIDs ) )
		if dbIDs != None:
			dbIDs = [int( dbID ) for dbID in dbIDs]

		# get display prefs so that we only get the skeleton data for the
		# stats this session's user wants
		userPrefs = self.getUserDisplayPrefs( logDb, sessionUID ).dict()
		if userPrefs['enabledProcStatOrder'].has_key( procType ):
			enabledStats = userPrefs['enabledProcStatOrder'][procType]
		else:
			defaultStats = self.getUserDisplayPrefs( logDb,	None ).dict()
			enabledStats = defaultStats['enabledProcStatOrder'][procType]

		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		procPref = prefTree.procPrefs[procType]
		cursor = dbManager.getLogDbCursor()

		getMultipleProcesses = (dbIDs == None or len(dbIDs) > 1)

		# get the column names
		columns = [ 'stat.tick', 'tick.time AS tickTime' ]

		if getMultipleProcesses:
			columns.append( "COUNT(stat.process) AS numProcesses" )
		else:
			columns.append( "1 AS numProcesses" )

		for enabledStat in enabledStats:
			statPref = procPref.statPrefById( int( enabledStat ) )
			if getMultipleProcesses:
				columns.append( statPref.consolidateColumn( "stat" ) )
			else:
				columns.append( "stat.%s" % statPref.columnName )

		statTableName = "%s_lvl_%03d" % (procPref.tableName, window.dbId)

		#apiLog.info( "getProcessStatisticsData: uid = %s", uid )
		query = "SELECT %s FROM %s stat " % \
			(", ".join( columns ), statTableName)
		query += "JOIN %s tick ON stat.tick = tick.id " % \
			(constants.tblStdTickTimes,)
		query += "JOIN %s proc ON stat.process = proc.id "\
			"WHERE proc.userid = %d " % \
				(constants.tblSeenProcesses, int( uid ))
		if dbIDs:
			query += "AND proc.id IN (%s) " % (", ".join( map( str, dbIDs ) ),)
		if fromTick and fromTick > 0:
			fromTick -= window.samplePeriodTicks
			query += "AND stat.tick > %d " % (fromTick,)
		if toTick:
			toTick += window.samplePeriodTicks
			query += "AND stat.tick < %d " % (toTick,)

		if getMultipleProcesses:
			query += "GROUP BY stat.tick "

		query += "ORDER BY stat.tick"

		sqlLog.debug(
			"getProcessStatisticsData query: %r", query )
		queryStart = time.time()
		cursor.execute( query )
		sqlLog.debug(
			" ...query took %.3fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )
		rows = cursor.fetchall()
		cursor.close()

		transformStart = time.time()
		apiLog.debug(
			"      Beginning query transform..." )
		stats = self.transformRows( enabledStats, rows )
		apiLog.debug(
			"      Query transform took %.3fs", time.time() - transformStart )
		apiLog.debug(
			"   getProcessStatisticsData finished: Took %.3fs",
				time.time() - funcStart )
		apiLog.info( "=" * 20 )

		return stats


	def getSkeletonData( self, logDb, uid, sessionUID, procType, dbIDs,
			fromTick = None ):
		"""
		Get the skeleton process statistics.

		@param logDb:		the log database name
		@param uid:			the user ID
		@param sessionUID:	the session user ID for getting user display prefs
		@param procType:	the process type
		@param dbIDs:		the database IDs
		@param fromTick:	(optional) specifies to get skeleton data from
							this tick, otherwise all available process
							statistic data is retrieved.
		"""
		dbManager, prefTree = managedb.ptManager.requestDbManager( logDb )
		# get the coarsest grain aggregation window
		coarsestWindow = prefTree.windowPrefs[-1]

		if fromTick is None:
			fromTick = 0
		return self.getProcessStatisticsData( logDb, uid, sessionUID, procType,
			dbIDs, coarsestWindow, fromTick, toTick = None )

	def transformRows( self, enabledStats, rows ):
		"""
		Given a result from rows, convert into a dictionary format
		which is more palatable for use in flash.
		"""
		statDict = {}

		statDict["times"] = []
		statDict["ticks"] = []
		statDict["num"] = []
		statDict["data"] = {}
		for statPref in enabledStats:
			statDict["data"][statPref] = []

		for row in rows:
			#processId = row[0]
			statDict["ticks"].append( row[0] )
			statDict["times"].append( row[1] )
			statDict["num"].append( row[2] )
			for i in range( len( enabledStats) ):
				statDict["data"][enabledStats[i]].append( row[3 + i] )

		#print statDict

		return statDict


	def getTickRange( self, dbManager, fromTime, toTime ):
		"""
		Given a start time and end time, return the ticks corresponding
		to that time range.

		Returns (fromTick, toTick).
		"""

		apiLog.debug( "getTickRange: fromTime = %f, toTime = %f (difference of %f)",
			fromTime, toTime, toTime - fromTime)

		cursor = dbManager.getLogDbCursor()
		fromTimeMs = int(fromTime)
		toTimeMs = int(toTime)

		# get the tick closest (approaching from below) to fromTime
		query = "SELECT tick1.id FROM %s tick1 WHERE tick1.time = " \
				" (SELECT MAX( tick2.time ) FROM %s tick2 "\
				"  WHERE tick2.time < %d)" % \
			(constants.tblStdTickTimes, constants.tblStdTickTimes, fromTimeMs)

		queryStart = time.time()
		sqlLog.debug( "getTickRange MAX query: %s", query )
		cursor.execute( query );
		sqlLog.debug( " ...query took %fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )

		row = cursor.fetchone()
		if row:
			fromTick = row[0]
		else:
			fromTick = None

		# get the tick closest (approaching from above) to toTime
		query = "SELECT tick1.id FROM %s tick1 WHERE tick1.time = " \
				" (SELECT MIN( tick2.time ) FROM %s tick2 "\
				"  WHERE tick2.time > %d)" % \
			(constants.tblStdTickTimes, constants.tblStdTickTimes, toTimeMs)

		sqlLog.debug( "getTickRange MIN query: %s", query )
		cursor.execute( query )
		sqlLog.debug( " ...query took %fs (%d rows)",
				(time.time() - queryStart), cursor.rowcount )
		row = cursor.fetchone()

		#apiLog.debug("getTickRange: Row is %s", row )
		if row:
			toTick = row[0]
		else:
			toTick = None

		apiLog.debug( "getTickRange: Returning values (%s, %s)",
			fromTick, toTick )

		return fromTick, toTick

# graph.py
