"""
A utility library containing data types and functions specifically for 
graph data request functionality.

Contains mostly container classes and AMF conversion functions.
"""
# Import third party libraries
from flashticle.amf import to_amf

# Import local libraries
import utils

# Import stat_logger modules
import prefclasses

# -------------------------------------------------------------------------
# Section: Container classes for serialisation
# -------------------------------------------------------------------------
class ProcessInfo:
	"""Container class for storing information about a process"""
	def __init__( self, dbId, pType, pName, pPid, mName ):
		self.dbId = dbId
		self.pType = pType
		self.name = pName
		self.pid = pPid
		self.machine = mName

	def __str__( self ):
		return "Process %s(%d) on %s" % (self.name, self.pid, self.machine)

class MachineInfo:
	"""Container class for storing information about a machine"""
	def __init__( self, ip, hostname ):
		self.ip = ip
		self.hostname = hostname

	def ipString( self ):
		return utils.inet_ntoa( self.ip )

	def __str__( self ):
		return "Machine %s (%s)" % \
			(self.hostname, self.ipString())

	def __repr__( self ):
		return str( self )

# -------------------------------------------------------------------------
# Section: AMF Conversion functions
# -------------------------------------------------------------------------
@to_amf.when( "isinstance(obj, MachineInfo)" )
def convertMachineInfoToAMF( machineInfo ):
	"""
	Called automatically by the AMF library when converting
	to a Flash Remoting stream, due to the decorator.
	"""
	return to_amf( dict (
		ip = machineInfo.ipString(),
		hostname = machineInfo.hostname
	) )

@to_amf.when( "isinstance(obj, ProcessInfo)" )
def convertProcessInfoToAMF( processInfo ):
	"""
	Called automatically by the AMF library when converting
	to a Flash Remoting stream, due to the decorator.
	"""
	return to_amf( dict (
		dbId = processInfo.dbId,
		type =  processInfo.pType,
		name = processInfo.name,
		pid = processInfo.pid,
		machine = processInfo.machine,
	) )

@to_amf.when("isinstance(obj, prefclasses.PrefTree)")
def convertPrefTreeToAMF( prefTree ):
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
def convertStatPrefToAMF( obj ):
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


@to_amf.when("isinstance(obj, prefclasses.WindowPref)")
def convertWindowPrefToAMF( obj ):
	wpDict = {}
	wpDict["samples"] = obj.samples
	wpDict["samplePeriodTicks"] = obj.samplePeriodTicks
	return to_amf( wpDict )
