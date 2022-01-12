from os import popen4
from os import execv
from os import path
from sys import argv
from sys import exit
from sys import stdout
from sys import stderr

CVS_PATH			=	"C:\\Program Files\\TortoiseCVS\\cvs.exe"
QUOTED_CVS_PATH		=	"\"" + CVS_PATH + "\""

FAILURE				=	3
SUCCESS				=	0

UNKNOWN				=	0
ADDED				=	1
REMOVED				=	2
UPTODATE			=	3
LOCALMODIFIED		=	4
NEEDCHECKOUT		=	5

COMMANDS = ( "addfolder", "addfile", "addbinaryfile", "removefile", "commitfile", "updatefile", "updatefolder", "refreshfolder", "managed" )

def printUsageAndAbort():
	print "USAGE: cvs_stub <" + '|'.join( COMMANDS ) + "> [file list]"
	exit( FAILURE )

if len( argv ) < 2 or not argv[1] in COMMANDS:
	printUsageAndAbort()

del argv[0]
command = argv[0]
del argv[0]

if command == "addfolder":
	del argv[0] # the message
	argv.insert( 0, "add" )
elif command == "addfile":
	argv.insert( 0, "add" )
elif command == "addbinaryfile":
	argv.insert( 0, "-kb" )
	argv.insert( 0, "add" )
elif command == "removefile":
	argv.insert( 0, "remove" )
elif command == "commitfile":
	argv.insert( 0, "-m" )
	argv.insert( 0, "-R" )
	argv.insert( 0, "commit" )
elif command == "updatefile":
	argv.insert( 0, "-C" )
	argv.insert( 0, "update" )
elif command == "updatefolder":
	argv.insert( 0, "update" )
elif command == "refreshfolder":
	argv.insert( 0, "-R" )
	argv.insert( 0, "status" )
elif command == "managed":
	if path.isdir( argv[ 0 ] ):
		if argv[ 0 ][ -1 : 0] != '/':
			argv[ 0 ] = argv[ 0 ] + '/'
		if path.isfile( argv[ 0 ] + "CVS/Entries" ):
			exit( 0 )
		exit( 1 )
	if path.isfile( argv[ 0 ] ):
		argv.insert( 0, "status" )
		argv.insert( 0, QUOTED_CVS_PATH )
		fds = popen4( ' '.join( argv ) )
		if fds[ 1 ]:
			output = ''.join( fds[ 1 ].readlines() )
			stderr.write( output )
			if output.find( "Unknown" ) == -1:
				exit( 0 )
	exit( 1 )
else:
	printUsageAndAbort()

argv.insert( 0, QUOTED_CVS_PATH )
exit( execv( CVS_PATH, argv ) )
