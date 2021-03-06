#!/usr/bin/env python

import bwsetup; bwsetup.addPath( ".." );

# We need our custom version of MySQLdb until both MySQLdb and
# CherryPy/TurboGears support automatic reconnection to the MySQL server,
# otherwise the web server will fall over unrecoverably should anything happen
# to the connection to the server.
bwsetup.addPath( "../pycommon/redist/mysql_python", 0 )

import sys
import os
import logging
import optparse
import MySQLdb

def main( configFile ):
	import pkg_resources
	pkg_resources.require( "TurboGears" )

	import turbogears
	import cherrypy
	cherrypy.lowercase_api = True

	from web_console.common import util
	from pycommon import log

	turbogears.update_config( configfile = configFile,
		modulename = "root.config" )

	try:
		import root.controllers
	except MySQLdb.OperationalError, e:
		log.error( "Couldn't connect to the MySQL database:\n%s", str( e ) )
		sys.exit( 1 )

	setupLoggers()
	util.verifyDojo()

	rootController = root.controllers.Root()

	# Make sure database schemas are up-to-date
	import web_console.common.model
	web_console.common.model.DictSQLObject.verifySchemas()

	turbogears.start_server( rootController )

def setupLoggers():
	logFormatter = logging.Formatter( "%(name)s %(asctime)s: %(message)s" )
	logFormatterNewline = \
		logging.Formatter( "\n%(name)s %(asctime)s: %(message)s" )

	# Setup stat_grapher logs
	# =========================================================================

	# Don't propgate stat_grapher logs up to the root logger (which typically
	# dumps to stdout)
	logging.getLogger( "stat_grapher" ).propagate = False

	sqlLog = logging.getLogger( "stat_grapher.sql" )
	sqlLogHandler = logging.FileHandler(
		"stat_grapher/stat_grapher_sql.log", "w")
	sqlLogHandler.setFormatter( logFormatterNewline )
	sqlLog.addHandler( sqlLogHandler )

	apiLog = logging.getLogger( "stat_grapher.api" )
	apiLogHandler = logging.FileHandler(
		"stat_grapher/stat_grapher_api.log", "w")
	apiLogHandler.setFormatter( logFormatter )
	apiLog.addHandler( apiLogHandler )

	amfLog = logging.getLogger( "stat_grapher.amf" )
	amfLogHandler = logging.FileHandler(
		"stat_grapher/stat_grapher_amf.log", "w")
	amfLogHandler.setFormatter( logFormatter )
	amfLog.addHandler( amfLogHandler )

	mdLog = logging.getLogger( "stat_grapher.md" )
	mdLogHandler = logging.FileHandler(
		"stat_grapher/stat_grapher_md.log", "w")
	mdLogHandler.setFormatter( logFormatter )
	mdLog.addHandler( mdLogHandler )

	# Setup log levels
	# =========================================================================
	logging.getLogger( "turbogears" ).setLevel( logging.INFO )
	logging.getLogger( "turbokid" ).setLevel( logging.INFO )
	logging.getLogger( "turbogears.identity" ).setLevel( logging.WARNING )
	logging.getLogger( "bigworld" ).setLevel( logging.WARNING )
	logging.getLogger( "stat_grapher" ).setLevel( logging.INFO )
	logging.getLogger( "stat_grapher.sql" ).setLevel( logging.DEBUG )
	logging.getLogger( "stat_grapher.amf" ).setLevel( logging.DEBUG )
	logging.getLogger( "stat_grapher.api" ).setLevel( logging.DEBUG )
	logging.getLogger( "stat_grapher.md" ).setLevel( logging.DEBUG )

	# We don't want all our pycommon log.* output twice
	from pycommon import log
	logging.getLogger( "bigworld" ).removeHandler( log.console )


def parseArgs():
	parser = optparse.OptionParser( usage="start-web_console.py [options] "
									"[CONFIG_FILE; default=dev.cfg]" )

	parser.add_option( "-d", "--daemon",
		action = "store_false", dest = "foreground",
		default = True,
		help = "run as daemon (default in foreground)" )

	homeDefault = os.path.abspath( os.path.dirname(sys.argv[0]) )
	parser.add_option( "", "--home",
		type = "string", dest = "home",
		default = homeDefault,
		help = "web console home directory (default '%default')" )

	parser.add_option( "-o", "--output",
		type = "string", dest = "output",
		help = "daemon output log file (default '%default')" )

	parser.add_option( "-e", "--error-output",
		type = "string", dest = "error_output",
		help = "daemon error log file (default '%default')" )

	parser.add_option( "-p", "--pid-file",
		type = "string", dest = "pid_file",
		default = "web_console.pid",
		help = "daemon PID file (default '%default')" )
	options, args = parser.parse_args()

	return options, args

if __name__ == "__main__":

	configFile = 'dev.cfg'

	options, args = parseArgs()
	if len( args ) > 0:
		configFile = args[0]

	try:
		import turbogears
	except ImportError, e:
		print "ERROR: Unable to import TurboGears module (%s)" % str(e)
		sys.exit( 1 )


	if options.foreground:

		# Redirect stdout/stderr if necessary
		if options.output:
			try:
				fd = os.open( options.output, os.O_CREAT | os.O_APPEND | os.O_WRONLY )
				os.dup2( fd, 1 )
				os.close( fd )
			except Exception, e:
				print "Error while attempting to redirect stdout to file '%s'" % options.error_output
				print str(e)

		if options.error_output:
			try:
				fd = os.open( options.error_output, os.O_CREAT | os.O_APPEND | os.O_WRONLY )
				os.dup2( fd, 2 )
				os.close( fd )
			except Exception, e:
				print "Error while attempting to redirect stderr to file '%s'" % options.error_output
				print str(e)

		main( configFile )
	else:

		if not options.output:
			options.output = "/dev/null"
		if not options.error_output:
			options.error_output = "/dev/null"

		from pycommon.daemon import Daemon
		d = Daemon( run = main,
			args = (configFile,),
			workingDir = options.home,
			outFile = options.output,
			errFile = options.error_output,
			pidFile = options.pid_file
		)
		d.start()
