#!/usr/bin/env python

import os
import sys
import pwd
import tempfile

usage = """
*****************
*** IMPORTANT ***
*****************

Please check the following before continuing:

1) Your sysadmin has entered you in the sudoers file on this box
2) You know what $CVSROOT is for your working environment
3) You know where your home directory will be on this box
4) You have shared the MF directory on your windows box

NOTE: If you are asked for a password, enter your *own* password, not the root
password.  This will not work unless you are in the sudoers file (see (1) above)
""".lstrip()

bashProfileTemplate = """
export PS1='[\u@\h \W]\$ '
export CVSROOT=%s
""".lstrip()

def prompt( s, default = "" ):
	if default:
		s += " [%s]" % default
	if "?" not in s:
		s += ": "
	else:
		s += " "
	sys.stdout.write( s )
	sys.stdout.flush()
	return sys.stdin.readline().rstrip() or default

def info( s ):
	print "INFO: " + s

def error( s ):
	print "ERROR: " + s

def fatal( s ):
	raise RuntimeError, "FATAL: " + s

def sudo_fwrite( text, fname ):
	fd, tmpname = tempfile.mkstemp()
	done = os.write( fd, text )
	if done != len( text ):
		error( "Only %d/%d bytes written to %s" % (done, len( text ), tmpname) )
		return False
	if os.system( "sudo cp %s %s" % (tmpname, fname) ):
		error( "Couldn't `sudo cp` %s -> %s" % (tmpname, fname) )
		return False
	return True

def main():

	print usage

	# Verify we are in the sudoers file
	if os.system( "sudo ls >/dev/null" ):
		fatal( "You must be in the sudoers list for this machine" )

	# Who are we configuring
	ent = pwd.getpwuid( os.getuid() )
	user = ent.pw_name

	# Make home dir if necessary
	if not os.path.exists( ent.pw_dir ):
		info( "Your home directory doesn't exist, will create one now ..." )
		default = "/home/%s" % ent.pw_name
		home = prompt( "Enter path for home directory", default )

		if os.system( "sudo mkdir -p %s" % home ):
			fatal( "Couldn't make home directory %s" % home )
		else:
			info( "Made %s" % home )

		if os.system( "sudo chown -R %d:%d %s" %
					  (ent.pw_uid, ent.pw_gid, home) ):
			fatal( "Couldn't make you owner of %s" % home )

		if home != default:
			os.chdir( "/home" )
			if os.system( "sudo ln -sf %s %s" % (home, ent.pw_dir) ):
				fatal( "Couldn't symlink %s -> %s" % (default, home) )
			else:
				info( "Symlinked %s -> %s" % (default, home) )
	else:
		home = ent.pw_dir

	# Set up MF directory
	while True:
		mfroot = prompt( "Enter location for MF directory", "%s/mf" % home )
		if os.path.exists( mfroot ) and not os.path.isdir( mfroot ):
			error( "%s already exists and is not a directory!" % mfroot )
			continue
		elif not os.path.isdir( os.path.dirname( mfroot ) ):
			error( "parent directory of %s does not exist!" % mfroot )
			continue
		else:
			break

	if not os.path.exists( mfroot ):
		try:
			os.mkdir( mfroot )
			info( "Made %s" % mfroot )
		except:
			fatal( "Couldn't make MF directory: %s" % mfroot )

	# Find out where the windows MF directory is shared
	winmfroot = prompt( "Enter Samba share name for your windows MF directory",
						"//%s/mf" % user )
	mounted = False
	while True:
		winmfmount = prompt( "Enter local mount point for windows MF directory",
							 "/mnt/mf-%s" % user )

		# Check if it is already correctly mounted
		grepout = os.popen( "mount | grep '%s on %s'" %
							(winmfroot, winmfmount) ).read().strip()
		if grepout:
			info( "Seems to be mounted already: %s" % grepout )
			if prompt( "Is this correct?", "y" ) == "y":
				mounted = True
				break

		# Check if something else is mounted at the mount point
		grepout = os.popen( "mount | grep 'on %s'" % winmfmount ).read().strip()
		if grepout:
			error( "Another filesystem already mounted at %s" % winmfmount )
			info( grepout )
			continue

		if os.path.isdir( winmfmount ) and os.listdir( winmfmount ):
			error( "%s already exists and is not empty" % winmfmount )
			continue
		if os.path.exists( winmfmount ) and not os.path.isdir( winmfmount ):
			error( "%s already exists and is not a directory" % winmfmount )
			continue
		break

	# Make the mount point if necessary
	if not os.path.isdir( winmfmount ):
		if os.system( "sudo mkdir -p %s" % winmfmount ):
			fatal( "Couldn't make mount point for windows MF directory: %s" %
				   winmfmount )
		else:
			info( "Made mount point: %s" % winmfmount )

	# Patch /etc/fstab if necessary
	orig_fstab = open( "/etc/fstab" ).read().rstrip()
	fstablines = orig_fstab.split( "\n" )
	for line in fstablines[:]:
		if winmfmount in line or winmfroot in line:
			fstablines.remove( line )
	fstablines.append( "%s\t%s\tcifs\tuid=%d,gid=%d,user=%s,"
					   "file_mode=0777,dir_mode=0777,noauto\t0 0\n" % \
					   (winmfroot, winmfmount, ent.pw_uid, ent.pw_gid, user) )

	# Try to write patched table back to /etc/fstab
	if sudo_fwrite( "\n".join( fstablines ), "/etc/fstab" ):
		info( "Patched /etc/fstab successfully" )
	else:
		fatal( "Couldn't patch /etc/fstab with your MF mountpoint" )

	# Attempt to mount the windows MF dir
	if mounted or os.system( "sudo mount %s" % winmfmount ) == 0:
		info( "%s is mounted at %s" % (winmfroot, winmfmount) )
	else:
		error( "Couldn't mount windows MF directory, restoring original fstab" )
		sudo_fwrite( orig_fstab, "/etc/fstab" )
		fatal( "Aborting due to previous errors" )

	# Detect which subdirs in $MF_ROOT have res trees
	resdirs = []
	for dirname in [os.path.join( winmfmount, f ) for f in
					os.listdir( winmfmount ) if
					os.path.isdir( os.path.join( winmfmount, f ) )]:
		if os.path.isdir( os.path.join( dirname, "res" ) ):
			resdirs.append( os.path.join( dirname, "res" ) )
	info( "The following res trees were detected on your windows share: " )
	for i in xrange( len( resdirs ) ):
		print "[%d] %s" % (i, resdirs[i])
	while True:
		choice = prompt( "Enter a comma-sep list of the dirs to link in", "all" )
		if choice == "all":
			selection = range( len( resdirs ) )
		else:
			try:
				selection = map( int, choice.split( "," ) )
			except:
				error( "Invalid input" )
				continue
		break

	# Link in the res trees
	projects = []
	for i in selection:
		resdir = resdirs[i]
		project = os.path.basename( os.path.dirname( resdir ) )
		projects.append( project )
		projdir = os.path.join( mfroot, project )
		reslink = os.path.join( projdir, "res" )

		if not os.path.isdir( projdir ) and \
			   os.system( "mkdir -p %s" % projdir ):
			fatal( "Couldn't create %s" % projdir )

		# If res tree is already a link to the required location, skip
		if os.path.islink( reslink ) and os.readlink( reslink ) == resdir:
			info( "%s is already a link to %s" % (reslink, resdir) )
			continue

		if os.path.exists( reslink ):
			fatal( "%s already exists and is not a link to %s" %
				   (reslink, resdir) )

		os.chdir( projdir )
		os.symlink( resdir, "res" )
		info( "Symlinked %s -> %s" % (reslink, resdir) )

	# Set up BW_RES_PATH
	while True:
		resorder = prompt( "Enter project names in order for your BW_RES_PATH",
						   ",".join( projects ) ).split( "," )
		bwrespath = ":".join( ["%s/%s/res" % (mfroot, proj)
							   for proj in resorder] )
		info( "BW_RES_PATH will be %s" % bwrespath )
		if prompt( "Hit [ENTER] to accept" ) == "":
			break

	# Set up CVSROOT
	cvsroot = prompt( "Please enter your CVSROOT",
					  ":pserver:%s@cvs:/usr/repository" % user )
	profile = "%s/.bash_profile" % home
	f = open( profile, "a" )
	f.write( bashProfileTemplate % cvsroot )
	f.close()
	info( "Wrote %s" % profile )

	# Make sure .bashrc sources .bash_profile
	rc = "%s/.bashrc" % home
	if not os.path.isfile( rc ) or \
	   ".bash_profile" not in open( rc ).read():
		open( rc, "a" ).write( ". ~/.bash_profile\n" )
		info( "~/.bashrc now sources ~/.bash_profile" )

	# Write ~/.bwmachined.conf
	open( "%s/.bwmachined.conf" % home, "a" ).\
		  write( "%s;%s\n" % (mfroot, bwrespath) )
	info( "Wrote %s/.bwmachined.conf" % home )

	# Ask user if he wants to do a checkout.  TODO: this only supports CVS at
	# the moment
	if prompt( "Fetch the server binaries from CVS?", "y" ) == "y":
		os.chdir( mfroot )
		os.environ[ "CVSROOT" ] = cvsroot
		if os.system( "cvs login" ):
			fatal( "Couldn't log into CVS repository" )

		cocmd = prompt( "Enter CVS checkout command",
						"cvs co -P bigworld/bin/Hybrid" )
		if os.system( cocmd ):
			fatal( "Couldn't check out server binaries from CVS" )

	info( "*** All done! ***" )

if __name__ == "__main__":
	try:
		sys.exit( main() )
	except KeyboardInterrupt:
		print "\n[terminated]"
	except RuntimeError, e:
		print e
