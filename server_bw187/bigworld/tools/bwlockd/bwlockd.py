#!/usr/bin/env python
"""
The BigWorld lock daemon process.
"""

# import daemon module from pycommon modules
import bwsetup
bwsetup.addPath( "../server" )

from pycommon.daemon import Daemon

# import libraries
import optparse
from socket import *
from select import select
from struct import *
from xml.dom.minidom import *
from time import time
from os import listdir, remove
import sys

# common definitions, should read from config file later
PORT = 8168
BACKLOG = 5
MAX_MESSAGE_LENGTH = 1024

# class definitions
class Rect: # not the same as Win32 RECT, since our right and bottom are inclusive
	def __init__( self, left, top, right, bottom ):
		self.left = left
		self.top = top
		self.right = right
		self.bottom = bottom
	def valid( self ):
		return self.left <= self.right and self.top <= self.bottom
	def isIn( self, x, y ):
		return self.left <= x and x <= self.right and self.top <= y and y <= self.bottom
	def intersect( self, other ):
		if self.isIn( other.left, other.top ) or self.isIn( other.right, other.top ):
			return 1
		if self.isIn( other.left, other.bottom ) or self.isIn( other.right, other.bottom ):
			return 1
		if other.isIn( self.left, self.top ) or other.isIn( self.right, self.top ):
			return 1
		if other.isIn( self.left, self.bottom ) or other.isIn( self.right, self.bottom ):
			return 1
		return 0
	def __repr__( self ):
		return '%d %d %d %d' % ( self.left, self.top, self.right, self.bottom )
	def __eval__( self, str ):
		values = str.split()
		if len( values ) != 4:
			raise Exception, "Format Error in Rect::__eval__"
		self.left = int( values[ 0 ] )
		self.top = int( values[ 1 ] )
		self.right = int( values[ 2 ] )
		self.bottom = int( values[ 3 ] )
	def __eq__( self, other ):
		return repr( self ) == repr( other )

class XMLWriter:
	def __init__( self, filename ):
		self.file = open( filename, 'w' )
		if not self.file:
			raise Exception, "Cannot open file " + filename
	def openTag( self, tagname, level ):
		for i in range( 0, level ):
			self.file.write( '\t' )
		self.file.write( '<' )
		self.file.write( tagname )
		self.file.write( '>\n' )

	def closeTag( self, tagname, level ):
		for i in range( 0, level ):
			self.file.write( '\t' )
		self.file.write( '</' )
		self.file.write( tagname )
		self.file.write( '>\n' )

	def tag( self, tagname, value, level ):
		for i in range( 0, level ):
			self.file.write( '\t' )
		self.file.write( '<' )
		self.file.write( tagname )
		self.file.write( '>' )
		self.file.write( '\t' )
		self.file.write( value )
		self.file.write( '\t' )
		self.file.write( '</' )
		self.file.write( tagname )
		self.file.write( '>\n' )
	def close( self ):
		self.file.close()

class Computer:
	def __init__( self, name ):
		self.name = name.lower()
		self.spaces = {}
		self.bypasssave = 0

	def filename( self ):
		return self.name + '.computer'

	def load( self ):
		self.bypasssave = 1
		self.spaces = {}
		dom = parse( self.filename() )
		for element in dom.documentElement.childNodes:
			if element.nodeName == 'space':
				spaceName = ''
				for sub in element.childNodes:
					if sub.nodeName == 'name':
						spaceName = sub.childNodes[0].nodeValue.strip().encode( 'ascii', 'replace' )
						break
				for sub in element.childNodes:
					if sub.nodeName == 'lock':
						rect = Rect( 0, 0, 0, 0 )
						desc = ''
						username = ''
						t = 0
						for subsub in sub.childNodes:
							if subsub.nodeName == 'rect':
								rect.__eval__( subsub.childNodes[0].nodeValue.strip() )
							elif subsub.nodeName == 'desc':
								desc = subsub.childNodes[0].nodeValue.strip().encode( 'ascii', 'replace' )
							elif subsub.nodeName == 'username':
								username = subsub.childNodes[0].nodeValue.strip().encode( 'ascii', 'replace' )
							elif subsub.nodeName == 'time':
								t = float( subsub.childNodes[0].nodeValue.strip() )
						self.addLock( spaceName, rect, username, desc, t )
		dom.unlink()
		self.bypasssave = 0

	def save( self ):
		if self.bypasssave:
			return
		writer = XMLWriter( self.filename() )
		rootName = self.name.replace( ' ', '_' )
		writer.openTag( rootName, 0 )

		for space in self.spaces.keys():
			writer.openTag( 'space', 1 )
			writer.tag( 'name', space, 2 )

			for lock in self.spaces[ space ]:
				writer.openTag( 'lock', 2 )
				writer.tag( 'rect', repr( lock[0] ), 3 )
				writer.tag( 'username', lock[1], 3 )
				writer.tag( 'desc', lock[2], 3 )
				writer.tag( 'time', repr( lock[3] ), 3 )
				writer.closeTag( 'lock', 2 )

			writer.closeTag( 'space', 1 )

		writer.closeTag( rootName, 0 )
		writer.close()

	def addLock( self, space, rect, username, desc, t = None ):
		if t == None:
			t = time()
		if not self.spaces.has_key( space ):
			self.spaces[ space ] = []
		for lock in self.spaces[ space ]:
			if lock[0] == rect:
				return 0
		self.spaces[ space ].append( ( rect, username, desc, t ) )
		self.save()
		return 1

	def removeLock( self, space, rect ):
		if self.spaces.has_key( space ):
			for lock in self.spaces[ space ]:
				if lock[0] == rect:
					self.spaces[ space ].remove( lock )
					self.save()
					return 1
		return 0

	def removeAllLocks( self ):
		self.spaces = {}
		self.save()

	def removeAllLocks( self, space ):
		if self.spaces.has_key( space ):
			del self.spaces[ space ]
			self.save()

	def spaceNum( self ):
		return len( self.spaces.keys() )

	def space( self, index ):
		return self.spaces.keys()[ index ]

	def lockNum( self, space ):
		if not self.spaces.has_key( space ):
			return 0
		return len( self.spaces[ space ] )
	def lock( self, space, index ): # lock is a triple ( rect, desc, time )
		return self.spaces[ space ][ index ]

	def intersect( self, space, rect ):
		if self.spaces.has_key( space ):
			for lock in self.spaces[ space ]:
				if lock[0].intersect( rect ):
					return 1
		return 0

class History:
	def __init__( self, filename, maxEntry = 100 ):
		self.filename = filename
		self.maxEntry = maxEntry
		self.bypasssave = 1
		self.records = []
		try:
			dom = parse( self.filename )
			for element in dom.documentElement.childNodes:
				if element.nodeName == 'record':
					name = ''
					desc = ''
					actor = ''
					workstation = ''
					t = ''
					for sub in element.childNodes:
						if sub.nodeName == 'name':
							name = sub.childNodes[0].nodeValue.strip().encode( 'ascii', 'replace' )
						elif sub.nodeName == 'desc':
							desc = sub.childNodes[0].nodeValue.strip().encode( 'ascii', 'replace' )
						elif sub.nodeName == 'actor':
							actor = sub.childNodes[0].nodeValue.strip().encode( 'ascii', 'replace' )
						elif sub.nodeName == 'workstation':
							workstation = sub.childNodes[0].nodeValue.strip().encode( 'ascii', 'replace' )
						elif sub.nodeName == 'time':
							t = float( sub.childNodes[0].nodeValue.strip() )
					self.addRecord( name, desc, actor, workstation, t )
			dom.unlink()
			self.bypasssave = 0
		except Exception, desc:
			self.bypasssave = 0
			self.save()

	def save( self ):
		if self.bypasssave:
			return
		writer = XMLWriter( self.filename )
		writer.openTag( 'history', 0 )

		for record in self.records:
			writer.openTag( 'record', 1 )
			writer.tag( 'name', record[0], 2 )
			writer.tag( 'desc', record[1], 2 )
			writer.tag( 'actor', record[2], 2 )
			writer.tag( 'workstation', record[3], 2 )
			writer.tag( 'time', repr( record[4] ), 2 )
			writer.closeTag( 'record', 1 )

		writer.closeTag( 'history', 0 )
		writer.close()

	def addRecord( self, name, desc, actor, workstation, t = None ):
		if t == None:
			t = time()
		if len( name ) and len( actor ) and len( workstation ):
			self.records.append( ( name, desc, actor, workstation, t ) )
			while len( self.records ) > self.maxEntry:
				self.records.pop( 0 )
			self.save()
		else:
			raise Exception, 'invalid parameters in History.addRecord' + \
				  ( 'name : %s, actor : %s, workstation : %s' % ( name, actor, workstation ) )

class Connection:
    def __init__( self, address, clientSocket ):
        self.pendingMessage = ''
        self.user = None
        self.computer = None
        self.address = address
        self.space = ''
        self.socket = clientSocket
    def addMessage( self, msg ):
        self.pendingMessage += msg
    def msgAvailable( self ):
        return len( self.pendingMessage ) > 4 and len( self.pendingMessage ) >= unpack( 'I', self.pendingMessage[0:4] )[ 0 ]
    def nextMsg( self ):
        msgSize = unpack( 'I', self.pendingMessage[0:4] )[ 0 ];
        msg = self.pendingMessage[:msgSize]
        self.pendingMessage = self.pendingMessage[msgSize:]
        return msg

class LockDaemon:
	def __init__( self, bindAddr, backlog ):
		self.computers = {}
		self.debug( 'loading history' )
		self.history = History( 'history.xml' )
		self.debug( 'loading computers' )
		files = listdir( '.' )
		for file in files:
			if file.lower()[-9:] == '.computer':
				try:
					c = Computer( file[:-9] )
					c.load()
					self.debug( 'loading computer %s' % c.name )
					self.computers[ c.name ] = c
				except:
					self.debug( 'loading computer %s failed' % file.lower()[:-4] )
		bindIP, bindPort = bindAddr
		self.debug( 'listening at %s:%d' % (bindIP, bindPort ) )
		self.serverSocket = socket( AF_INET, SOCK_STREAM )
		self.serverSocket.setsockopt( SOL_SOCKET, SO_REUSEADDR, 1 )
		self.serverSocket.bind( bindAddr )
		self.serverSocket.listen( backlog )
		self.readList = [ self.serverSocket ]
		self.writeList = []
		self.exceptionList = []

		self.connections = {}

	def debug( self, msg ):
		print msg
		sys.stdout.flush()

	def step( self ):
		[ readResult, writeResult, exceptionResult ] = select( self.readList, self.writeList, self.exceptionList )
		if self.serverSocket in readResult:
			( clientSocket, clientAddress ) = self.serverSocket.accept()
			try:
				remoteAddr = gethostbyaddr( clientAddress[0] )
				self.onConnect( clientSocket, remoteAddr[0] )
			except Exception, msg:
				self.debug( "%s (from %s)" % (msg, clientAddress) )
		else:
			connection = self.connections[ readResult[0] ]
			msg = ''

			try:
				msg = readResult[0].recv( MAX_MESSAGE_LENGTH )
			except Exception, msg:
				msg = ''

			if msg == '':
				self.onDisconnect( connection )
			else:
				connection.addMessage( msg )
				while connection.msgAvailable():
					msg = connection.nextMsg()
					self.onMessage( msg, connection )

	def sendMessage( self, clientSocket, code, failed, msg, disconnect = 1 ):
		try:
			self.debug( "%s %s %s" % (msg, code, failed) )
			msg = pack( 'IcB', len( msg ) + 6, code, failed % 256 ) + msg;
			clientSocket.sendall( msg )
		except Exception, msg:
			self.debug( msg )
			if disconnect:
				self.onDisconnect( self.connections[ clientSocket ] )

	def addRecord( self, name, desc, connection, username = None ):
		if username == None:
			if connection.user != None:
				username = connection.user
			else:
				username = '(unknown)'
		self.history.addRecord( name, desc, username, connection.address )

	def onConnect( self, clientSocket, clientAddress ):
		if len( clientAddress ) > 0:
			clientAddress = clientAddress.split( '.' )[ 0 ].lower()
		for connection in self.connections.values():
			if connection.address == clientAddress:
				self.debug( "%s already connected, disconnected" % clientAddress )
				self.sendMessage( clientSocket, 'C', 1, 'already connected', 0 )
				clientSocket.close()
		else:
			self.readList.append( clientSocket )
			self.connections[ clientSocket ] = Connection( clientAddress, clientSocket )
			if not clientAddress in self.computers.keys():
				self.computers[ clientAddress ] = Computer( clientAddress )
			self.connections[ clientSocket ].computer = self.computers[ clientAddress ]
			self.sendMessage( clientSocket, 'C', 0, 'hello from bwlockd at ' + gethostname() )
			self.debug( "%s connected : %s" % (clientAddress, clientAddress[ 0 ] ) )

	def onDisconnect( self, connection ):
		self.debug( "%s disconnected" % connection.address )
		self.readList.remove( connection.socket )
		del self.connections[ connection.socket ]
		connection.socket.close()

	def onMessage( self, msg, connection ):
		if len( msg ) < 6:
			self.debug( 'got invalid message from %s' % connection.address )
			self.addRecord( 'invalidmessage', repr( msg ), connection )
			return

		code = msg[4:5]

		if code == 'A':
			self.onUsername( msg[6:], connection )
		elif code == 'S':
			self.onSetSpace( msg[6:], connection )
		elif code == 'L':
			if len( msg ) < 14:
				self.debug( 'got invalid message from %s' % connection.address )
				self.addRecord( 'invalidmessage', repr( msg ), connection )
				return
			( minx, miny, maxx, maxy ) = unpack( 'hhhh', msg[6:14] )
			self.onLockChunk( Rect( minx, miny, maxx, maxy ), msg[14:], connection )
		elif code == 'U':
			if len( msg ) < 14:
				self.debug( 'got invalid message from %s' % connection.address )
				self.addRecord( 'invalidmessage', repr( msg ), connection )
				return
			( minx, miny, maxx, maxy ) = unpack( 'hhhh', msg[6:14] )
			self.onUnlockChunk( Rect( minx, miny, maxx, maxy ), msg[14:], connection )
		elif code == 'G':
			self.onGetStatus( connection )

	def onUsername( self, username, connection ):
		connection.user = username
		self.sendMessage( connection.socket, 'A', 0, 'login succeeded' )
		self.addRecord( 'login', 'login succeeded', connection )

	def onSetSpace( self, space, connection ):
		connection.space = space
		self.sendMessage( connection.socket, 'S', 0, 'set space to ' + space )

	def onLockChunk( self, rect, desc, connection ):
		if len( connection.space ) == 0:
			self.sendMessage( connection.socket, 'L', 1, 'space not set' )
		elif len( desc ) == 0:
			self.sendMessage( connection.socket, 'L', 1, 'must have desc' )
		elif connection.user == None:
			self.sendMessage( connection.socket, 'L', 1, 'user not set' )
		elif not rect.valid():
			self.sendMessage( connection.socket, 'L', 1, 'invalid rect' + repr( rect ) )
		else:
			for computer in self.computers.values():
				if computer != connection.computer and computer.intersect( connection.space, rect ):
					self.sendMessage( connection.socket, 'L', 1, 'conflict with computer ' + computer.name )
					return
			if connection.computer.addLock( connection.space, rect, connection.user, desc ) == 0:
				self.sendMessage( connection.socket, 'L', 1, 'locked rect failed : ' + repr( rect ) + ' already existed' )
			else:
				self.sendMessage( connection.socket, 'L', 0, 'locked rect ' + repr( rect ) )
				self.addRecord( 'lock', 'locked rect ' + repr( rect ) + ' for ' + desc, connection )
				connections = self.connections.values()[:]
				msg = pack( 'hhhh', rect.left, rect.top, rect.right, rect.bottom )
				msg += pack( 'I', len( connection.computer.name ) ) + connection.computer.name
				msg += pack( 'I', len( connection.user ) ) + connection.user
				msg += pack( 'I', len( desc ) ) + desc
				msg += pack( 'f', time() )
				for c in connections:
					if c.space == connection.space and c.user != None:
						self.sendMessage( c.socket, 'l', 0, msg )


	def onUnlockChunk( self, rect, desc, connection ):
		if len( connection.space ) == 0:
			self.sendMessage( connection.socket, 'U', 1, 'space not set' )
		elif len( desc ) == 0:
			self.sendMessage( connection.socket, 'U', 1, 'must have desc' )
		elif connection.user == None:
			self.sendMessage( connection.socket, 'U', 1, 'user not set' )
		elif not rect.valid():
			self.sendMessage( connection.socket, 'U', 1, 'invalid rect : ' + repr( rect ) )
		else:
			if connection.computer.removeLock( connection.space, rect ):
				self.sendMessage( connection.socket, 'U', 0, 'unlocked rect ' + repr( rect ) )
				self.addRecord( 'unlock', 'unlocked rect ' + repr( rect ) + ' for ' + desc, connection )
				connections = self.connections.values()[:]
				msg = pack( 'hhhh', rect.left, rect.top, rect.right, rect.bottom )
				msg += pack( 'I', len( connection.computer.name ) ) + connection.computer.name
				msg += pack( 'I', len( connection.user ) ) + connection.user
				msg += pack( 'I', len( desc ) ) + desc
				msg += pack( 'f', time() )
				for c in connections:
					if c.space == connection.space and c.user != None:
						self.sendMessage( c.socket, 'u', 0, msg )
			else:
				self.sendMessage( connection.socket, 'U', 1, 'rect not locked : ' + repr( rect ) )

	def onGetStatus( self, connection ):
		if len( connection.space ) == 0:
			self.sendMessage( connection.socket, 'G', 1, 'space not set' )
		elif connection.user == None:
			self.sendMessage( connection.socket, 'G', 1, 'user not set' )
		else:
			msg = ''
			for computer in self.computers.values():
				if computer.lockNum( connection.space ) != 0:
					record = pack( 'I', len( computer.name ) ) + computer.name + pack( 'I', computer.lockNum( connection.space ) )
					for index in range( 0, computer.lockNum( connection.space ) ):
						lock = computer.lock( connection.space, index )
						rect = lock[0]
						username = lock[1]
						desc = lock[2]
						time = lock[3]
						record = record + pack( 'hhhh', rect.left, rect.top, rect.right, rect.bottom )
						record = record + pack( 'I', len( username ) ) + username
						record = record + pack( 'I', len( desc ) ) + desc
						record = record + pack( 'f', time )
					msg = msg + pack( 'I', len( record ) ) + record
			self.sendMessage( connection.socket, 'G', 0, msg )

def lockDaemonMain( bindAddr ):
	daemon = LockDaemon( bindAddr, BACKLOG )
	while True:
		daemon.step()
	return 0

# main program
if __name__ == "__main__":
	parser = optparse.OptionParser( usage="%prog [options]")
	parser.add_option( '', '--daemon',
		action='store_true',
		dest='daemon', default=False,
		help="run process in the background" )

	parser.add_option( '-I', '--bind-ip',
		dest="bind_ip",
		default=gethostname(),
		help="IP/hostname of interface to bind to (default %default)" )

	parser.add_option( '-p', '--port',
		dest="bind_port",
		default=PORT,
		type="int",
		help="port to listen on (default %default)" )

	parser.add_option( '', '--pid',
		dest="pid_file",
		default="bwlockd.pid",
		help="default daemon PID file (default %default)" )

	parser.add_option( '-o', '--output',
		dest="out_file",
		default="bwlockd.log",
		help="default daemon output file (default %default)" )

	options, args = parser.parse_args()

	bindAddr = (options.bind_ip, options.bind_port)
	if options.daemon:
		daemon = Daemon( run=lockDaemonMain,
			args=(bindAddr, ),
			outFile=options.out_file,
			errFile=options.out_file,
			pidFile=options.pid_file )
		daemon.start()
		sys.exit()
	else:
		sys.exit( lockDaemonMain( bindAddr ) )
