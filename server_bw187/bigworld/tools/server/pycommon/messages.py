"""
An interface to various network protocols implemented by BigWorld server
components and daemons.
"""

import struct
import os
import socket
import random
import select

import socketplus
import util
import memory_stream
import log

# TODO We aren't really using this at the moment
useMulticast = os.environ.has_key( "BW_MULTICAST" )

# ------------------------------------------------------------------------------
# Section: MGMPacket
# ------------------------------------------------------------------------------

class MGMPacket( object ):

	MAX_SIZE = 32768
	PACKET_STAGGER_REPLIES = 0x1

	def __init__( self, stream = None ):
		self.flags = 0
		self.buddy = 0
		self.messages = []
		if stream:
			self.read( stream )

	def __str__( self ):
		sbuddy = socket.inet_ntoa( struct.pack( "I", self.buddy ) )
		return "Flags: 0x%x\n" % self.flags + \
			   "Buddy: %s\n" % sbuddy + \
			   "\n".join( [str( mgm ) for mgm in self.messages] )

	def read( self, stream ):
		self.messages = []
		self.flags, self.buddy = stream.unpack( "BI" )
		while stream.remainingLength() > 0:
			msglen, = stream.unpack( "H" )
			msgstream = memory_stream.MemoryStream( stream.read( msglen ) )
			if msgstream.remainingLength() != msglen:
				log.error( "Only %d bytes on stream, needed %d",
						   msgstream.remainingLength(), msglen )
				raise stream.error

			mgm = MachineGuardMessage.create( msgstream )
			if mgm:
				self.append( mgm )
			else:
				mgm = UnknownMessage()
				msgstream.seek( 0 )
				mgm.read( msgstream )
				self.append( mgm )
				log.error( "Unknown message received:\n %s" % mgm )

	def write( self, stream ):
		stream.pack( ("BI", self.flags, self.buddy) )
		for mgm in self.messages:
			pSize = stream.tell()
			stream.seek( 2, 1 )
			sizeBefore = stream.tell()
			mgm.write( stream )
			sizeAfter = stream.tell()
			stream.seek( pSize )
			stream.pack( ("H", sizeAfter - sizeBefore) )
			stream.seek( sizeAfter )

	def append( self, mgm ):
		self.messages.append( mgm )

	def get( self ):
		stream = memory_stream.MemoryStream()
		self.write( stream )
		return stream.data()

	def set( self, s ):
		stream = memory_stream.MemoryStream( s )
		self.read( stream )

# ------------------------------------------------------------------------------
# Section: WholeMachineMessage
# ------------------------------------------------------------------------------

class MachineGuardMessage( object ):

	MACHINED_PORT = 20018
	MACHINED_VERSION = 39

	# Message types
	WHOLE_MACHINE_MESSAGE = 1
	PROCESS_MESSAGE = 2
	PROCESS_STATS_MESSAGE = 3
	LISTENER_MESSAGE = 4
	CREATE_MESSAGE = 5
	SIGNAL_MESSAGE = 6
	TAGS_MESSAGE = 7
	USER_MESSAGE = 8
	PID_MESSAGE = 9
	RESET_MESSAGE = 10
	ERROR_MESSAGE = 11

	# Flags
	MESSAGE_DIRECTION_OUTGOING = 0x1
	MESSAGE_NOT_UNDERSTOOD = 0x2

	messageMap = {}
	seqTicker = random.randint( 0, 65536 )

	def __init__( self, message, flags = 0, seq = 0 ):
		self.message = message
		self.flags = flags
		self.seq = seq


	def __str__( self ):
		return "Message: %s\n" % self.messageMap[ self.message ] + \
			   "Flags: %x\n" % self.flags + \
			   "Seq: %d" % self.seq


	@util.synchronized
	def refreshSeq( self ):
		"""
		Generates a new sequence number for this message.
		"""

		MachineGuardMessage.seqTicker = (MachineGuardMessage.seqTicker + 1) % 0xffff
		self.seq = MachineGuardMessage.seqTicker


	def write( self, stream ):
		"""
		As in machine_guard.cpp, calling this method will generate a new
		sequence number for this message, to prevent ever sending the same
		sequence number twice.  This of course assumes that calling this method
		precedes sending this message (a fairly safe assumption I think).
		"""

		self.refreshSeq()
		stream.pack( ("BBH", self.message, self.flags, self.seq) )


	def read( self, stream ):
		try:
			self.message, self.flags, self.seq = stream.unpack( "BBH" )
			return True
		except stream.error:
			return False


	def send( self, sock, addr = "<broadcast>" ):
		try:
			packet = MGMPacket()
			packet.append( self )
			if addr == "<broadcast>":
				packet.flags = packet.PACKET_STAGGER_REPLIES
			stream = memory_stream.MemoryStream()
			packet.write( stream )
			sock.sendto( stream.getvalue(), (addr, self.MACHINED_PORT) )
			return True
		except stream.error:
			return False


	@classmethod
	def create( self, stream ):

		# Set up message id -> subclass mapping if it isn't done yet
		if not self.messageMap:
			self.messageMap[ self.WHOLE_MACHINE_MESSAGE ] = WholeMachineMessage
			self.messageMap[ self.PROCESS_MESSAGE ] = ProcessMessage
			self.messageMap[ self.PROCESS_STATS_MESSAGE ] = ProcessStatsMessage
			self.messageMap[ self.LISTENER_MESSAGE ] = ListenerMessage
			self.messageMap[ self.CREATE_MESSAGE ] = CreateMessage
			self.messageMap[ self.SIGNAL_MESSAGE ] = SignalMessage
			self.messageMap[ self.TAGS_MESSAGE ] = TagsMessage
			self.messageMap[ self.USER_MESSAGE ] = UserMessage
			self.messageMap[ self.PID_MESSAGE ] = PidMessage
			self.messageMap[ self.RESET_MESSAGE ] = ResetMessage
			self.messageMap[ self.ERROR_MESSAGE ] = ErrorMessage

		# Just peek the message type and then defer to derived read() impls
		try:
			message = stream.peek()
		except struct.error:
			log.error( "Empty MGM stream" )
			return None

		try:
			subclass = self.messageMap[ message ]
		except:
			log.error( "Unknown message id %d" % message )
			return None

		# Try to destream
		mgm = subclass()
		if mgm.read( stream ):
			return mgm
		else:
			log.error( "Couldn't read %s from stream" %
							 mgm.__class__.__name__ )
			return None


	@classmethod
	def batchQuery( self, mgms, timeout = 1.0, machines = [], attempts = 3 ):
		"""
		Send each mgm query to each machine and await the response.  If no
		machines are specified, mgms are sent broadcast.

		If machines are specified, this method will return as soon as each
		machine has replied.  If messages were sent broadcast, this method will
		return when we know each machined has replied (from the buddy system).
		In either case, this method will also return if the timeout expires.

		A mapping from input mgm -> [ (replymgm, srcaddr) ... ]	is returned.

		This method is efficient whether you are expecting one or many replies.
		"""

		sock = socketplus.socket( "bm" )

		for i in xrange( attempts ):

			# Pool of addresses we have received replies from
			replied = set()

			# Pool of addresses we are expecting replies from
			if machines:
				waiting = set( [m.ip for m in machines] )
			else:
				waiting = set()

			# Make packet with all the requests on it
			packet = MGMPacket()
			for mgm in mgms:
				packet.append( mgm )
			if not machines:
				packet.flags = packet.PACKET_STAGGER_REPLIES
			stream = memory_stream.MemoryStream()
			packet.write( stream )

			if machines:
				for m in machines:
					sock.sendto( stream.data(), (m.ip, self.MACHINED_PORT) )
			else:
				sock.sendto(
					stream.data(), ("<broadcast>", self.MACHINED_PORT) )

			# Return map
			ret = {}
			for mgm in mgms:
				ret[ mgm ] = []

			# Receive replies until we're done or until timeout
			while True:
				if not select.select( [sock], [], [], timeout )[0]:
					break

				data, srcaddr = sock.recvfrom( MGMPacket.MAX_SIZE )
				stream = memory_stream.MemoryStream( data )

				try:
					packet = MGMPacket( stream )
					replies = packet.messages
				except stream.error:
					log.error( "Garbage packet on stream from %s:%d", *srcaddr )
					util.hexdump( stream.getvalue() )
					continue

				# Map each reply to its corresponding request
				for reply in replies:

					# Drop message if the server didn't understand the request
					if reply.flags & reply.MESSAGE_NOT_UNDERSTOOD:
						log.error(
							"Dropping reply to %s because %s didn't understand it",
							reply.__class__.__name__, srcaddr[0] )
						continue

					try:
						query = [mgm for mgm in mgms if reply.seq == mgm.seq][0]
					except:
						log.warning( "Received bad seq # (wanted one of %s) "
									 "from %s:\n%s",
									 [m.seq for m in mgms], srcaddr[0], reply )
						continue

					if isinstance( reply, ErrorMessage ):
						log.cmsg( reply.severity, reply.msg )

					ret[ query ].append( (reply, srcaddr) )

				replyip = srcaddr[0]
				buddyip = socket.inet_ntoa( struct.pack( "I", packet.buddy ) )
				replied.add( replyip )
				if replyip in waiting:
					waiting.remove( replyip )
				if not machines and (buddyip not in replied and packet.buddy != 0):
					waiting.add( buddyip )

				# Break early if we know we're done
				if replied and not waiting:
					break

			if waiting:
				if i < attempts-1:
					log.warning( "Still waiting for %s after %d tries",
								 ",".join( waiting ), i+1 )
				else:
					log.error( "No MGM replies from %s after %d tries",
							   ",".join( waiting), attempts )

			if waiting and i < attempts-1:
				continue

			return ret


	def query( self, machine, timeout = 1.0, attempts = 3 ):
		"""
		Convenience method for querying a single machine with a single MGM.

		Returns a list of replies or [] if timed out.
		"""
		replies = self.batchQuery( [self], timeout, [machine], attempts )
		if replies[ self ]:
			return [mgm for mgm, addr in replies[ self ]]
		else:
			return []


class WholeMachineMessage( MachineGuardMessage ):

	class IfStats( object ):

		# Taken from common_machine_guard.hpp
		MAX_BIT_RATE = 1 << 27
		BIT_INCREMENT = MAX_BIT_RATE / 0xFF
		MAX_PACKET_RATE = 256000
		PACK_INCREMENT = MAX_PACKET_RATE / 0xFF

		def __init__( self, name="", bitsIn=0, bitsOut=0, packIn=0, packOut=0 ):

			self.name = name

			# We translate these values now to make things easier for
			# stat_logger/stat_grapher.  Since we never write non-zero values
			# for these, it's not too much of a problem, even if it is
			# out-of-sync wrt/ reading and writing sides.
			self.bitsIn = bitsIn * self.BIT_INCREMENT
			self.bitsOut = bitsOut * self.BIT_INCREMENT
			self.packIn = packIn * self.PACK_INCREMENT
			self.packOut = packOut * self.PACK_INCREMENT

		def __str__( self ):
			return "%s: %d/%d bit/s %d/%d pack/s" % \
				   (self.name, self.bitsIn, self.bitsOut,
					self.packIn, self.packOut)

		def __nonzero__( self ):
			return self.bitsIn or self.bitsOut or self.packIn or self.packOut

		def write( self, stream ):
			stream.pack( self.name,
						 ("BBBB", self.bitsIn, self.bitsOut,
						  self.packIn, self.packOut) )


	def __init__( self ):
		MachineGuardMessage.__init__(
			self, MachineGuardMessage.WHOLE_MACHINE_MESSAGE )
		self.hostname = ""
		self.cpuSpeed = 0
		self.nCpus = 0
		self.nInterfaces = 0
		self.cpuLoads = []
		self.mem = 0
		self.ifStats = {}
		self.inDiscards = self.outDiscards = 0
		self.version = 0

	def __str__( self ):
		return super( WholeMachineMessage, self ).__str__() + "\n" + \
			   "Hostname: %s\n" % self.hostname + \
			   "CPUs: %dMHz x %d\n" % (self.cpuSpeed, self.nCpus) + \
			   "Mem usage: %d%%\n" % (self.mem/2.55) + \
			   "CPU loads: %s\n" % " ".join( ["%d%%" % (x/2.55) for x in
											  self.cpuLoads] ) + \
			   "\n".join( [str( x ) for x in self.ifStats.itervalues()] ) + "\n" + \
			   "Discards: %d/%d\n" % (self.inDiscards, self.outDiscards) + \
			   "Version: %d" % self.version

	def read( self, stream ):
		if not super( WholeMachineMessage, self ).read( stream ):
			return False
		try:
			self.hostname, self.cpuSpeed, self.nCpus, self.nInterfaces, \
						   self.mem, self.version, self.inDiscards, \
						   self.outDiscards = stream.unpack( "s", "HBBBBBB" )
			self.cpuLoads = [stream.unpack( "B" )[0]
							 for i in xrange( self.nCpus )]

			ifStatList = [self.IfStats( *stream.unpack( "s", "BBBB" ) ) for i in xrange( self.nInterfaces )]
			self.ifStats = dict((ifStat.name, ifStat) for ifStat in ifStatList )
			return True

		except stream.error:
			return False

	def write( self, stream ):
		super( WholeMachineMessage, self ).write( stream )
		stream.pack( self.hostname,
					 ("HBBBBBB", self.cpuSpeed, self.nCpus, self.nInterfaces,
					  self.mem, self.version, self.inDiscards,
					  self.outDiscards) )
		for i in xrange( self.nCpus ):
			stream.pack( ("B", self.cpuLoads[i]) )

		assert( len(self.ifStats) == self.nInterfaces )
		for ifStat in self.ifStats.itervalues():
			ifStat.write( stream )

class ProcessMessage( MachineGuardMessage ):

	SERVER_COMPONENT = 0
	WATCHER_NUB = 1

	PARAM_USE_CATEGORY = 0x1
	PARAM_USE_UID = 0x2
	PARAM_USE_PID = 0x4
	PARAM_USE_PORT= 0x8
	PARAM_USE_ID = 0x10
	PARAM_USE_NAME = 0x20
	PARAM_IS_MSGTYPE = 0x80

	REGISTER = 1
	DEREGISTER = 2
	NOTIFY_BIRTH = 3
	NOTIFY_DEATH = 4

	def __init__( self ):
		MachineGuardMessage.__init__(
			self, MachineGuardMessage.PROCESS_MESSAGE )
		self.param = 0
		self.category = 0
		self.uid = 0
		self.pid = 0
		self.port = 0
		self.id = 0
		self.name = ""

	def __str__( self ):
		return super( ProcessMessage, self ).__str__() + "\n" + \
			   "Param: %x\n" % self.param + \
			   "Category: %d\n" % self.category + \
			   "Name: %s\n" % self.name + \
			   "PID: %d\n" % self.pid + \
			   "UID: %d\n" % self.uid + \
			   "ID: %d\n" % self.id + \
			   "Port: %d" % self.port

	def read( self, stream ):
		if not super( ProcessMessage, self ).read( stream ):
			return False
		try:
			self.param, self.category, self.uid, self.pid, \
					   self.port, self.id, self.name = \
					   stream.unpack( "BBHHHH", "s" )
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( ProcessMessage, self ).write( stream )
		stream.pack( ("BBHHHH", self.param, self.category, self.uid,
					  self.pid, self.port, self.id), self.name )

class ProcessStatsMessage( ProcessMessage ):

	def __init__( self ):
		ProcessMessage.__init__( self )
		self.message = MachineGuardMessage.PROCESS_STATS_MESSAGE
		self.load = 0
		self.mem = 0

	def __str__( self ):
		return super( ProcessStatsMessage, self ).__str__() + "\n" + \
			   "Load: %d%%\n" % (self.load / 2.55) + \
			   "Mem: %d%%" % (self.mem / 2.55)

	def read( self, stream ):
		if not super( ProcessStatsMessage, self ).read( stream ):
			return False
		try:
			self.load, self.mem = stream.unpack( "BB" )
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( ProcessStatsMessage, self ).write( stream )
		stream.pack( ("BB", self.load, self.mem) )

class ListenerMessage( ProcessMessage ):

	ADD_BIRTH_LISTENER = 0
	ADD_DEATH_LISTENER = 1

	def __init__( self ):
		ProcessMessage.__init__( self )
		self.message = MachineGuardMessage.LISTENER_MESSAGE
		self.preAddr = ""
		self.postAddr = ""

	def read( self, stream ):
		if not super( ListenerMessage, self ).read( stream ):
			return False
		try:
			self.preAddr, self.postAddr = stream.unpack( "s", "s" )
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( ListenerMessage, self ).write( stream )
		stream.pack( self.preAddr, self.postAddr )

class CreateMessage( MachineGuardMessage ):

	def __init__( self ):
		MachineGuardMessage.__init__(
			self, MachineGuardMessage.CREATE_MESSAGE )
		self.name = None
		self.config = "Hybrid"
		self.uid = None
		self.recover = 0
		self.fwdIp = 0
		self.fwdPort = 0

	def __str__( self ):
		return super( CreateMessage, self ).__str__() + "\n" + \
			   "Name: %s\n" % self.name + \
			   "Config: %s\n" % self.config + \
			   "UID: %d\n" % self.uid + \
			   "Recover: %s\n" % bool( self.recover ) + \
			   "Forward Addr: %s:%d" % (socket.inet_ntoa( struct.pack(
			"I", self.fwdIp ) ), socket.ntohs( self.fwdPort ))

	def read( self, stream ):
		if not super( CreateMessage, self ).read( stream ):
			return False
		try:
			self.name, self.config, self.uid, self.recover, self.fwdIp, \
					   self.fwdPort = stream.unpack( "s", "s", "HBIH" )
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( CreateMessage, self ).write( stream )
		stream.pack( self.name, self.config, ("HBIH", self.uid, self.recover,
											  self.fwdIp, self.fwdPort) )

class SignalMessage( ProcessMessage ):

	SIGINT = 2
	SIGQUIT = 3
	SIGUSR1 = 10

	def __init__( self ):
		ProcessMessage.__init__( self )
		self.message = MachineGuardMessage.SIGNAL_MESSAGE
		self.signal = None

	def __str__( self ):
		return super( SignalMessage, self ).__str__() + "\n" + \
			   "Signal: %d" % self.signal

	def read( self, stream ):
		if not super( SignalMessage, self ).read( stream ):
			return False
		try:
			self.signal, = stream.unpack( "B" )
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( SignalMessage, self ).write( stream )
		stream.pack( ("B", self.signal) )

class TagsMessage( MachineGuardMessage ):

	def __init__( self ):
		MachineGuardMessage.__init__(
			self, MachineGuardMessage.TAGS_MESSAGE )
		self.tags = []
		self.exists = 0

	def __str__( self ):
		return super( TagsMessage, self ).__str__() + "\n" + \
			   "Tags: %s\n" % self.tags + \
			   "Exists: %s" % bool( self.exists )

	def read( self, stream ):
		if not super( TagsMessage, self ).read( stream ):
			return False
		try:
			size, self.exists = stream.unpack( "BB" )
			self.tags = [stream.unpack( "s" )[0] for i in xrange( size )]
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( TagsMessage, self ).write( stream )
		stream.pack( ("BB", len( self.tags ), self.exists) )
		for tag in self.tags:
			stream.pack( tag )

class UserMessage( MachineGuardMessage ):

	PARAM_USE_UID = 0x1
	PARAM_USE_NAME = 0x2
	PARAM_CHECK_COREDUMPS = 0x4
	PARAM_REFRESH_ENV = 0x8
	PARAM_SET = 0x80

	UID_NOT_FOUND = 0xffff

	def __init__( self ):
		MachineGuardMessage.__init__(
			self, MachineGuardMessage.USER_MESSAGE )
		self.param = 0
		self.uid = 0
		self.username = ""
		self.fullname = ""
		self.home = ""
		self.mfroot = ""
		self.bwrespath = ""
		self.coredumps = []

	def __str__( self ):
		return super( UserMessage, self ).__str__() + "\n" + \
			   "Param: %x\n" % self.param + \
			   "UID: %d\n" % self.uid + \
			   "Username: %s\n" % self.username + \
			   "Fullname: %s\n" % self.fullname + \
			   "Home: %s\n" % self.home + \
			   "MF_ROOT: %s\n" % self.mfroot + \
			   "BW_RES_PATH: %s\n" % self.bwrespath + \
			   "Coredumps: %s" % self.coredumps

	def read( self, stream ):
		if not super( UserMessage, self ).read( stream ):
			return False
		try:
			self.param, self.uid, self.username, self.fullname, \
			self.home, self.mfroot, self.bwrespath = \
			stream.unpack( "BH", "s", "s", "s", "s", "s" )

			# NOTE: This is basically an inline implementation of the
			# std::vector streaming operators from binary_stream.hpp.  The next
			# time an MGM needs this, we should really implement standardised
			# support for it in memory_stream.py
			nCores, = stream.unpack( "I" )
			self.coredumps = [stream.unpack( "s", "s", "I" )
							  for i in xrange( nCores )]
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( UserMessage, self ).write( stream )
		stream.pack( ("BH", self.param, self.uid), self.username,
					 self.fullname, self.home, self.mfroot, self.bwrespath )

		stream.pack( ("I", len( self.coredumps )) )
		for filename, assrt, time in self.coredumps:
			stream.pack( filename, assrt, ("I", time) )


class UnknownMessage( MachineGuardMessage ):

	def __init__( self ):
		self.data = ""

	def __str__( self ):
		return "Data: %s" % util.hexdump( self.data, False )

	def read( self, stream ):
		if not super( UnknownMessage, self ).read( stream ):
			return False
		try:
			self.data = stream.read()
			self.flags = self.flags | self.MESSAGE_NOT_UNDERSTOOD
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( UnknownMessage, self ).write( stream )
		stream.write( self.data )

class PidMessage( MachineGuardMessage ):

	def __init__( self ):
		MachineGuardMessage.__init__( self, MachineGuardMessage.PID_MESSAGE )
		self.pid = 0
		self.running = False

	def __str__( self ):
		return super( PidMessage, self ).__str__() + "\n" + \
			   "PID: %d\n" % self.pid + \
			   "Running: %s" % self.running

	def read( self, stream ):
		if not super( PidMessage, self ).read( stream ):
			return False
		try:
			self.pid, self.running = stream.unpack( "HB" )
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( PidMessage, self ).write( stream )
		stream.pack( ("HB", self.pid, self.running) )

class ResetMessage( MachineGuardMessage ):

	def __init__( self ):
		MachineGuardMessage.__init__( self, MachineGuardMessage.RESET_MESSAGE )

class ErrorMessage( MachineGuardMessage ):

	def __init__( self ):
		MachineGuardMessage.__init__( self, MachineGuardMessage.ERROR_MESSAGE )
		self.severity = 0
		self.msg = ""

	def __str__( self ):
		return super( ErrorMessage, self ).__str__() + "\n" + \
			   "Severity: %d\n" % self.severity + \
			   "Message: %s\n" % self.message

	def read( self, stream ):
		if not super( ErrorMessage, self ).read( stream ):
			return False
		try:
			self.severity, self.msg = stream.unpack( "B", "s" )
			return True
		except stream.error:
			return False

	def write( self, stream ):
		super( ErrorMessage, self ).write( stream )
		stream.pack( ("B", self.severity), self.message )

# ------------------------------------------------------------------------------
# Section: WatcherDataMessage
# ------------------------------------------------------------------------------

class WatcherDataMessage:
	WATCHER_MSG_GET = 16
	WATCHER_MSG_SET = 17
	WATCHER_MSG_TELL = 18

	STRUCT_FORMAT = "<II"

	# The number of times we'll try to requery a Watcher value
	REQUERY_MAX = 2

	def __init__(self):
		self.message = 0
		self.count = 0
		self.strings = []
		self.extensionStream = None

	def get( self ):
		"""
		Convert the fields of this message into a stream.
		"""

		if self.extensionStream:
			packet = struct.pack( "I", self.message ) + \
					 self.extensionStream.data()

		else:
			packet = struct.pack( self.STRUCT_FORMAT, self.message, self.count )
			for s in self.strings:
				packet += s + '\0'

		return packet

	def set( self, str ):
		"""
		Unpack the contents of a stream into the fields of this message.
		"""

		(self.message, self.count) = struct.unpack( self.STRUCT_FORMAT,
													str[0:8] )
		offset = 8

		nums = self.count
		if self.message != self.WATCHER_MSG_GET:
			nums = nums + nums
		for i in xrange( nums ):
			next = str.index( '\0', offset )
			self.strings.append( str[ offset:next ] )
			offset = next+1

		return self

	def __str__(self):
		s = "WatcherDataMessage:\n"
		s += " message = %d\n" % self.message
		s += " count = %d\n" % self.count
		ix = 0
		for i in xrange(0,self.count):
			s += "  string %d: %s\n" % (i, self.strings[ ix ])
			ix += 1
			if self.message != self.WATCHER_MSG_GET:
				s += "  result %d: %s\n" % (i, self.strings[ ix ])
				ix += 1
		return s

	def addGetRequest( self, s ):
		self.strings.append( s )
		self.count += 1

	def addSetRequest( self, s1, s2):
		self.strings.append( s1 )
		self.strings.append( s2 )
		self.count += 1

	def getExtensionStream( self, message ):
		"""
		Marks this message as a watcher extension message, using the provided
		extension message ID.  Returns a MemoryStream that the extension data
		should be packed into.
		"""

		self.message = message
		self.extensionStream = memory_stream.MemoryStream()
		return self.extensionStream

	def getReply( self, index ):
		return (self.strings[ index+index ], self.strings[ index+index+1 ])

	@staticmethod
	def query( path, proc, timeout ):
		"""
		Light overload for batchQuery(), returns list of replies
		"""

		replies = WatcherDataMessage.batchQuery( [path], [proc], timeout )

		if proc in replies:
			return replies[ proc ].get( path, [] )
		else:
			return []

	@staticmethod
	def batchQuery( paths, procs, timeout ):
		"""
		Batch retrieval of a watcher value from one or more processes.
		Return value is:

		{process: {request-path: [(path1,val1), (path2,val2), ...]}}
		"""

		sock = socketplus.socket( "m" )

		procs = set( procs )
		paths = set( paths )

		wdm = WatcherDataMessage()
		wdm.message = wdm.WATCHER_MSG_GET

		for path in paths:
			wdm.addGetRequest( path )

		# Mapping from process -> number of times failed to reply in time
		notReplied = {}

		# Mapping from (ip, port) -> process to correlate replies
		addresses = {}

		# Mapping from process -> list of {watcher path -> watcher response}
		replies = {}

		# Send out all the requests and initialise dictionaries
		for p in procs:
			try:
				sock.sendto( wdm.get(), p.addr() )

			# These errors are caught on the sendto() call
			except socket.error:
				continue
			except TypeError:
				continue

			notReplied[ p ] = 0
			replies[ p ] = {}
			addresses[ p.addr() ] = p

		# Collect all the replies
		while notReplied:

			# This inner loop iterates until all replies received or timeout
			while notReplied and select.select( [sock], [], [], timeout )[0]:

				data, (srcip, srcport) = sock.recvfrom( 65536 )
				try:
					reply = WatcherDataMessage()
					reply.set( data )
				except:
					log.error( "Corrupt WDM reply from %s" % srcip )
					log.error( util.hexdump( data ) )
					continue

				# If it's not a TELL and not an MGM either, start worrying
				if reply.message != reply.WATCHER_MSG_TELL:
					log.error( "Got an unrecognised watcher message" )
					log.error( str( reply ) )
					continue

				# Lookup process and get its reply map
				try:
					p = addresses[ (srcip, srcport) ]
				except KeyError:
					log.warning( "Ignoring unexpected watcher response from " \
						"process at %s:%d" % (srcip, srcport) )
					continue

				pmap = replies[ p ]

				# Go through messages and sort them into the correct reply buckets
				for i in xrange( reply.count ):
					replyPath, replyValue = reply.getReply( i )

					# First look for an exact match in request paths (ie for
					# value requests)
					if replyPath in paths:
						pmap.setdefault( replyPath, [] ).\
							append( (replyPath, replyValue) )
						continue

					# Next look for dirname match (ie for directory contents)
					dirname = os.path.dirname( replyPath )
					if dirname in paths:
						pmap.setdefault( dirname, [] ).\
							append( (replyPath, replyValue) )
						continue

					log.error( "Incorrect reply (%s,%s) for %s" % \
							   (replyPath, replyValue, paths) )

				# Mark this process as having replied
				try:
					del notReplied[ p ]
				except KeyError:
					# In rare situations we get two responses from one process,
					# where one response is probably a delayed response
					# from an earlier query - so we can't delete from
					# this dictionary twice.
					log.warning( "Possibly delayed packet from process %s", p )

			# If we're not done yet, chances are a reply was lost, send out the
			# request again, hopefully we'll get lucky this time
			for p, n in notReplied.items():
				log.warning( "%s on %s:%d didn't reply to WDM (%d/%d)",
							 p.name, p.machine.name, p.port(), n + 1,
							 WatcherDataMessage.REQUERY_MAX )
				sock.sendto( wdm.get(), p.addr() )
				notReplied[ p ] += 1

				# If a process has failed too many times, just put in an empty
				# list for its replies
				if notReplied[ p ] >= WatcherDataMessage.REQUERY_MAX:
					del notReplied[ p ]
					replies[ p ] = {}

		return replies

# ------------------------------------------------------------------------------
# Section: LoggerComponentMessage
# ------------------------------------------------------------------------------

class LoggerComponentMessage( object ):

	MESSAGE_LOGGER_VERSION = 6
	FORMAT = "BBHI"

	def __init__( self, uid, componentName ):
		self.version = LoggerComponentMessage.MESSAGE_LOGGER_VERSION
		self.loggerID = 0
		self.uid = uid
		self.pid = os.getpid()
		self.componentName = componentName

	def write( self, stream ):
		stream.pack( (self.FORMAT, self.version, self.loggerID, self.uid,
					  self.pid), self.componentName )

	def read( self, stream ):
		(self.version, self.loggerID, self.uid, self.pid,
		 self.componentName) = stream.unpack( self.FORMAT, "s" )


# ------------------------------------------------------------------------------
# Section: QueryInterfaceMessage
# ------------------------------------------------------------------------------

# TODO: This message has not yet been implemented in python because its
#       functionality isn't currently required.
