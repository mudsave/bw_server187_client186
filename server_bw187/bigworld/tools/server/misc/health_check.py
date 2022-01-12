#!/usr/bin/env python

import os
import sys
import optparse
import smtplib
import socket
import time

import bwsetup; bwsetup.addPath( ".." )

from pycommon import cluster
from pycommon import uid as uidmodule
from pycommon import log

CC = "%s/../control_cluster.py" % bwsetup.appdir
FAIL_FILE = "%s/.health_check_failed" % bwsetup.appdir
DOMAIN = "bigworldtech.com"

uid = uidmodule.getuid()
username = uidmodule.getname( uid )

opt = optparse.OptionParser( "Usage: %prog <username>" )
opt.add_option( "-u", dest = "uid", default = uid,
				help = "UID or username of the BigWorld server to check on" )
opt.add_option( "-t", "--to", dest = "to",
				default = "root@%s" % DOMAIN,
				help = "Address to send mail to in event of failure" )
opt.add_option( "-f", "--from", dest = "fromaddr",
				default = "%s@%s" % (username, DOMAIN),
				help = "Address to send mail to in event of failure" )
opt.add_option( "-m", "--message", dest = "message",
				default = "Wahhhhhhhhhh!",
				help = "Message text in email" )
opt.add_option( "--no-mail", action = "store_true",
				help = "Don't send email on failure" )
(options, args) = opt.parse_args()

# If the failure file is too old, blow it away
if os.path.exists( FAIL_FILE ) and \
   time.time() - os.stat( FAIL_FILE ).st_ctime > 24 * 60 * 60:
	os.unlink( FAIL_FILE )

if not os.system( "%s -u %s check > /dev/null" % (CC, options.uid) ):

	# Remove failure file
	if os.path.exists( FAIL_FILE ):
		os.unlink( FAIL_FILE )

	sys.exit( 0 )

# If failure file exists, abort now
if os.path.exists( FAIL_FILE ):
	log.warning( "Bailing with 0 exit status due to recent "
				 ".health_check_failed file" )
	sys.exit( 0 )


# Send email
if not options.no_mail:

	message = ("""
From: %s
To: %s
Subject: %s's server on %s is down!

%s""" % (options.fromaddr, options.to, username,
		 socket.gethostname(), options.message)).lstrip()

	server = smtplib.SMTP( "localhost" )
	server.sendmail( options.fromaddr, options.to, message )
	server.quit()

	log.info( "Sent email to %s", options.to )


# Touch a file in this directory to say that we're in an error state
open( FAIL_FILE, "w" ).close()
