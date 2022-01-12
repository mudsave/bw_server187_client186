#!/usr/bin/env python

import sys
import os
import time

import bwlog
import util

import bwsetup; bwsetup.addPath( ".." )
from pycommon import log

FLAGS = { "d": bwlog.SHOW_DATE,
		  "t": bwlog.SHOW_TIME,
		  "h": bwlog.SHOW_HOST,
		  "u": bwlog.SHOW_USER,
		  "i": bwlog.SHOW_PID,
		  "a": bwlog.SHOW_APPID,
		  "p": bwlog.SHOW_PROCS,
		  "s": bwlog.SHOW_SEVERITY,
		  "m": bwlog.SHOW_MESSAGE }

TIME_FMT = "%a %d %b %Y %H:%M:%S"

USAGE = """%prog [options] [logdir]

Dump log output to the console.

By default, output is dumped in a `cat`-like manner.  With the -f switch, output
can be continuously dumped in a `tail -f`-like manner.

The times for which output is dumped can be constrained using the --from, --to,
and --around switches.  The arguments to each command can either be a literal
date in the format 'Thu 09 Nov 2006 16:09:01', or a file, whose last modified
time will be used.

The columns shown in the output can be manually configured using the -c switch,
which accepts a string of characters, each of which turns on a single column.
Note that the ordering of the output columns is fixed, and cannot be set by
passing flags to -c in a different order to the default.

The accepted flags are:

	d: SHOW_DATE
	t: SHOW_TIME
	h: SHOW_HOST
	u: SHOW_USER
	i: SHOW_PID
	a: SHOW_APPID
	p: SHOW_PROCS
	s: SHOW_SEVERITY
	m: SHOW_MESSAGE

--narrow is an alias for '-c tpsm'.

--filter is an advanced option that allows you to utilise the filtering
capabilities of the message_logger API.  Please see QueryParams::QueryParams()
in bwlog.cpp for more info on the required format/types of the arguments.
Currently supported options include:

  host pid appid procs severities message interpolate casesens direction

Please note that this is advanced functionality and should only be used if you
*REALLY* know what you're doing.  Without knowledge of the inner workings of
bwlog.cpp you will probably hit type errors.  Piping output through `grep`
should be enough to provide filtering for most situations.

As a quick example, to show only PID 2216, pass `--filter pid 2216`.
""".rstrip()

def main():

	opt = util.getBasicOptionParser( USAGE )

	opt.add_option( "-f", "--follow", dest = "follow", action = "store_true",
					help = "`tail -f`-like behaviour" )
	opt.add_option( "-i", "--interval", dest = "interval", default = 1.0,
					type = "float",
					help = "Refresh interval when in follow mode" )
	opt.add_option( "-c", "--cols",
					help = "Set columns to be shown in output" )
	opt.add_option( "-s", "--start", action = "store_true",
					help = "Only show logs after last server startup" )
	opt.add_option( "-n", "--narrow", action = "store_const", dest = "cols",
					const = "tpsm",
					help = "An alias for '-c tpsm'" )
	opt.add_option( "--from", dest = "ffrom",
					help = "Specify start time" )
	opt.add_option( "--to",
					help = "Specify end time" )
	opt.add_option( "--around",
					help = "Specify time of interest" )
	opt.add_option( "--context", type = "int", default = 20,
					help = "Specify lines of context when using --around or "
					"--follow" )
	opt.add_option( "--filter", action = "callback", callback = addFilter,
					nargs = 2, type = "str",
					help = "Add filter options to the fetch() call" )

	mlog, kwargs, options, args = util.basicInit( opt )

	# Insert manually specified filters if appropriate
	if hasattr( opt, "filters" ):
		kwargs.update( opt.filters )

	if options.cols:
		flags = sum( [FLAGS[ c ] for c in options.cols] )
	else:
		flags = bwlog.SHOW_ALL

	# Find last server restart if required
	if options.start:
		ul = mlog.getUserLog( options.uid )
		startup = ul.getLastServerStartup()
		if startup:
			kwargs[ "startaddr" ] = startup[ "addr" ]
		else:
			log.warning( "No server startup in log, starting from beginning" )

	# For --around
	if options.around:

		if not parseTime( options.around, "start", kwargs ):
			return 1

		# Need to increment to get expected behaviour
		options.context += 1

		# Display backwards context first
		results, query = mlog.fetchContext( kwargs, max = options.context )
		for result in results:
			print result.format( flags ),

		return cat( mlog, flags, kwargs, max = options.context, query = query )

	else:
		if options.ffrom and not parseTime( options.ffrom, "start", kwargs ):
			return 1

		if options.to and not parseTime( options.to, "end", kwargs ):
			return 1

	if options.follow:
		return tail( mlog, flags, options.interval, kwargs, options.context )
	else:
		return cat( mlog, flags, kwargs )


def addFilter( options, opt, args, parser ):

	if not hasattr( parser, "filters" ):
		parser.filters = {}

	filter, value = args

	try:
		value = eval( value )
	except:
		pass

	parser.filters[ filter ] = value


def parseTime( s, name, kw ):
	"""
	Write the time represented by 's' as kw[ name ].
	"""

	if os.path.isfile( s ):
		t = os.stat( s ).st_mtime
		kw[ name ] = t
		log.info( "File timestamp is %s",
				  time.strftime( TIME_FMT, time.localtime( t ) ) )

	else:
		try:
			kw[ name ] = time.mktime( time.strptime( s, TIME_FMT ) )

		except ValueError:
			log.error( "Time format must be like 'Thu 09 Nov 2006 16:09:01'" )
			return False

	return True


def cat( mlog, flags, kwargs, max = None, query = None ):
	"""
	Dump the contents of a user's log to stdout.  An already-started query can
	be passed in using the 'query' argument, otherwise a new one will be started
	using the kwargs.
	"""

	if not query:
		query = mlog.fetch( **kwargs )

	i = 0

	for result in query:
		if max and i >= max:
			break
		print result.format( flags ),
		i += 1

	return True


def tail( mlog, flags, interval, kwargs, context ):
	"""
	Display log output from a user's log in a similar fashion to `tail -f`.
	"""

	kwargs[ "start" ] = time.time()

	# Display some context
	context, query = mlog.fetchContext( kwargs, max = context )
	for entry in context:
		print entry.format( flags ),

	# Now start tailing
	while True:
		query.waitForResults( interval )
		mlog.dump( query, flags )

	return True


if __name__ == "__main__":
	try:
		sys.exit( main() )
	except IOError, e:
		if e.errno != 32: # Broken pipe
			raise e
	except KeyboardInterrupt:
		pass
