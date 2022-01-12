from os import popen4
from os import execv
from os import path
from os import spawnv
from os import P_WAIT
from sys import argv
from sys import exit
from sys import stdout
from sys import stderr

SVN_PATH			=	"C:\\Program Files\\Subversion\\bin\\svn.exe"
QUOTED_SVN_PATH		=	"\"" + SVN_PATH + "\""

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
	print "USAGE: svn_stub <" + '|'.join( COMMANDS ) + "> [file list]"
	exit( FAILURE )

if len( argv ) < 2 or not argv[1] in COMMANDS:
	printUsageAndAbort()

del argv[0]
command = argv[0]
del argv[0]

if command == "addfolder":
	addargv = argv[:]
	del addargv[0] # the message
	addargv.insert( 0, "-N" )
	addargv.insert( 0, "add" )
	addargv.insert( 0, QUOTED_SVN_PATH )
	if spawnv( P_WAIT, SVN_PATH, addargv ) == 0:
		argv.insert( 0, "-m" )
		argv.insert( 0, "-N" )
		argv.insert( 0, "commit" )
	else:
		exit( 1 )
elif command == "addfile":
	argv.insert( 0, "add" )
elif command == "addbinaryfile":
	argv.insert( 0, "add" )
elif command == "removefile":
	argv.insert( 0, "remove" )
elif command == "commitfile":
	argv.insert( 0, "-m" )
	argv.insert( 0, "commit" )
	argv[2] = "\"" + argv[2] + "\""
elif command == "updatefile":
	argv.insert( 0, "revert" )
elif command == "updatefolder":
	argv.insert( 0, "update" )
elif command == "refreshfolder":
	pass
elif command == "managed":
	if path.isdir( argv[ 0 ] ):
		if argv[ 0 ][ -1 : 0] != '/':
			argv[ 0 ] = argv[ 0 ] + '/'
		if path.isfile( argv[ 0 ] + ".svn/entries" ):
			exit( 0 )
		exit( 1 )
	if path.isfile( argv[ 0 ] ):
		argv.insert( 0, "-v" )
		argv.insert( 0, "status" )
		argv.insert( 0, QUOTED_SVN_PATH )
		fds = popen4( ' '.join( argv ) )
		if fds[ 1 ]:
			output = ''.join( fds[ 1 ].readlines() )
			stderr.write( output )
			if len(output) != 0 and output[0] != '?':
				exit( 0 )
	exit( 1 )
else:
	printUsageAndAbort()

argv.insert( 0, QUOTED_SVN_PATH )
exit( execv( SVN_PATH, argv ) )
