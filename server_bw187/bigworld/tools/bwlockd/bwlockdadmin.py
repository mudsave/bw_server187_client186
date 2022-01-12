from bwlockd import *
from sys import *

class BWLockdClient:
	def __init__( self, server, port ):
		self.socket = socket( AF_INET, SOCK_STREAM )
		self.socket.connect( ( server, port ) )
		print 'connected to', server + ':' + repr( port )
		self.getMessage()

	def sendMessage( self, code, msg ):
		msg = pack( 'IcB', len( msg ) + 6, code, 0 ) + msg;
		self.socket.sendall( msg )

	def getMessage( self ):
		msg = ''
		while 1:
			msg = msg + self.socket.recv( 1024 )
			if len( msg ) > 4 and len( msg ) == unpack( 'I', msg[0:4] )[ 0 ]:
				print msg[6:]
				break

	def createUser( self, name, permission ):
		self.sendMessage( 'C', name + ' ' + permission )
		self.getMessage()

	def deleteUser( self, name ):
		self.sendMessage( 'D', name )
		self.getMessage()

	def userPermission( self, name, permission ):
		self.sendMessage( 'P', name + ' ' + permission )
		self.getMessage()

	def setUsername( self, username ):
		self.sendMessage( 'A', username )
		self.getMessage()

	def setSpace( self, space ):
		self.sendMessage( 'S', space )
		self.getMessage()

	def lock( self, rect, desc ):
		self.sendMessage( 'L', pack( 'hhhh', rect.left, rect.top, rect.right, rect.bottom ) + desc )
		self.getMessage()

	def unlock( self, rect, desc ):
		self.sendMessage( 'U', pack( 'hhhh', rect.left, rect.top, rect.right, rect.bottom ) + desc )
		self.getMessage()

	def getStatus( self ):
		pass

def test():
	client = BWLockdClient( 'polyrandom', 8168 )

	client.createUser( 'xiaoming', 'test' )
	client.createUser( 'xiaoming1', 'all' )
	client.deleteUser( 'xiaoming1' )
	client.userPermission( 'xiaoming', 'all' )

	client.setUsername( 'admin' )
	client.createUser( 'xiaoming', 'test' )
	client.createUser( 'xiaoming1', 'all' )
	client.deleteUser( 'xiaoming1' )
	client.userPermission( 'xiaoming', 'all' )

	client.setUsername( 'xiaoming' )
	client.lock( Rect( 1, 2, 3, 4 ), 'first lock' )
	client.lock( Rect( 1, 2, -3, 4 ), 'second lock' )
	client.lock( Rect( 2, 3, 5, 8 ), 'third lock' )
	client.unlock( Rect( 1, 2, 3, 4 ), 'first unlock' )
	client.unlock( Rect( 1, 2, -3, 4 ), 'second unlock' )
	client.unlock( Rect( 2, 3, 5, 8 ), 'third unlock' )

	client.setSpace( 'testspace' )
	client.lock( Rect( 1, 2, 3, 4 ), 'first lock' )
	client.lock( Rect( 1, 2, -3, 4 ), 'second lock' )
	client.lock( Rect( 2, 3, 5, 8 ), 'third lock' )
	client.lock( Rect( 2, 3, 5, 8 ), 'fourth lock' )
	client.unlock( Rect( 1, 2, 3, 4 ), 'first unlock' )
	client.unlock( Rect( 1, 2, -3, 4 ), 'second unlock' )
	client.unlock( Rect( 2, 3, 5, 8 ), 'third unlock' )

	client.lock( Rect( 1, 2, 3, 4 ), 'first lock' )
	client.lock( Rect( 2, 3, 5, 8 ), 'third lock' )
	client.setUsername( 'admin' )
	client.lock( Rect( 2, 3, 10, 18 ), 'admin first lock' )
	client.lock( Rect( 100, 200, 300, 400 ), 'admin second lock' )

	client.getStatus()

	return 0

def main():
	return test()

# main program
if __name__ == "__main__":
	exit( main() )
