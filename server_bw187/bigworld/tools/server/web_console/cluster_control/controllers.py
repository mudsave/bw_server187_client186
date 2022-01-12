import logging
import cherrypy
import turbogears
import sqlobject

from turbogears import controllers, expose, redirect
from turbogears import validate, validators, identity
from turbogears import widgets
from turbojson import jsonify

# Standard python modules
from StringIO import StringIO
import os
import re
import random
import threading
import traceback

# BigWorld modules
import bwsetup; bwsetup.addPath( "../.." )
from pycommon import cluster
from pycommon import uid as uidmodule
from pycommon import log
from pycommon import async_task
import pycommon.util
from web_console.common import util
from web_console.common import module
from web_console.common import ajax
import model

class ClusterControl( module.Module ):

	def __init__( self, *args, **kw ):
		module.Module.__init__( self, *args, **kw )
		self.addPage( "Manage Servers", "procs" )
		self.addPage( "All Users", "users" )
		self.addPage( "All Machines", "machines" )
		self.addPage( "Saved Layouts", "layouts" )
		self.addPage( "Help", "help", popup=True )

	@identity.require( identity.not_anonymous() )
	@expose( template="common.templates.help_panes" )
	def help( self ):
		return dict( PAGE_TITLE="BigWorld WebConsole: ClusterControl Help" )

	@identity.require( identity.not_anonymous() )
	@expose( template="common.templates.help_left_pane" )
	def helpLeftPane( self ):
		return dict( section="cluster_control" )

	def appendToLayout( self, layout, tag ):
		for i in xrange( len( layout ) ):
			layout[i] = layout[i] + (tag,)


	@identity.require(identity.not_anonymous())
	@validate( validators = dict( id=validators.Int() ) )
	@ajax.expose
	def poll( self, id, blocking ):
		return util.poll( id, blocking )

	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.procs" )
	def procs( self, user=None ):
		c = cluster.Cluster()
		user = util.getUser( c, user )

		if user:
			procs = sorted( user.getProcs() )
		else:
			procs = []

		return dict( procs = procs, user = user,
					 page_specific_js = ["/log/static/js/query.js"] )


	@identity.require(identity.not_anonymous())
	@expose()
	def index( self ):
		raise redirect( turbogears.url( "procs" ) )

	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.machine" )
	def machine( self, machine ):
		c = cluster.Cluster()
		machine = c.getMachine( machine )
		procs = sorted( machine.getProcs() )
		procs.sort( key = lambda p: p.uid )
		return dict( m = machine, ps = procs )

	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.machines" )
	def machines( self, group = None ):
		c = cluster.Cluster()
		machines = c.getMachines()
		machines.sort()

		if group:
			c.queryTags( "Groups" )
			machines = [m for m in machines if m.tags.has_key( "Groups" )
						and group in m.tags[ "Groups" ]]

		return dict( ms = machines, group = group )

	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.users" )
	def users( self, user=None ):

		c = cluster.Cluster()
		activeUsers = sorted( c.getUsers() )
		inactiveUsers = {}
		for user in uidmodule.getall():
			inactiveUsers[ user.name ] = user
		for user in activeUsers:
			if inactiveUsers.has_key( user.name ):
				del inactiveUsers[ user.name ]
		inactiveUsers = sorted( inactiveUsers.values() )

		return dict( activeUsers = activeUsers,
					 inactiveUsers = inactiveUsers )


	# Perform the same functionality as the control_cluster 'flush' command.
	# This should only be called from the "All Users" page, and then return
	# the user to that page with an updated user list.
	@identity.require( identity.not_anonymous() )
	@expose()
	def usersFlush( self ):

		c = cluster.Cluster()
		ms = c.getMachines()

		for m in ms:
			m.flushMappings()

		raise redirect( "users" )


	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.watcher" )
	def watcher( self, machine, pid, path="", newval=None ):

		c = cluster.Cluster()
		m = c.getMachine( machine )
		p = m.getProc( int( pid ) )
		if not p:
			raise redirect( "/error", msg = "Process %s no longer exists" % pid )
		wd = p.getWatcherData( path )

		if newval:
			status = wd.set( newval )
			return dict( p = p, m = m, status = str( status ),
						 wd = p.getWatcherData( os.path.dirname( wd.path ) ) )
		else:
			return dict( p = p, m = m, status = `True`, wd = wd )


	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.start" )
	def start( self, user ):
		c = cluster.Cluster()
		try:
			user = c.getUser( user, refreshEnv = True, checkCoreDumps = True )
		except Exception, e:
			raise redirect( "/error", msg = str(e) )

		try:
			prefs = model.ccStartPrefs.select(
				model.ccStartPrefs.q.userID == util.getSessionUID() )[0]
		except:
			prefs = model.ccStartPrefs( user = util.getSessionUser(),
										mode = "single",
										arg = "",
										useTags = True )

		# It is possible that 'arg' will be saved to the DB as NULL when
		# starting a cluster by group name and selecting '(use all machines)'.
		# If this occurs we must force the arg to be an empty string so the
		# KID template doesn't fail.
		if prefs.arg == None:
			prefs.arg = ""

		savedLayouts = model.ccSavedLayouts.select(
			model.ccSavedLayouts.q.userID == util.getSessionUID() )

		return dict( user = user, c = c, prefs = prefs,
					 savedLayouts = [x.name for x in savedLayouts] )


	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.startproc" )
	@validate( validators = dict( count=validators.Int() ) )
	def startproc( self, user=None, pname=None, machine=None, count=None ):

		c = cluster.Cluster()
		user = util.getUser( c, user )

		if pname and machine and count:
			pname = pname.encode( "ascii" )
			user.startProc( c.getMachine( machine ), pname, count )
			raise redirect( "procs", user = user.name )

		machines = c.getMachines()
		machines.sort(lambda x, y: cmp(x.name, y.name))

		return dict( user = user, machines = machines )

	@identity.require(identity.not_anonymous())
	@expose()
	@validate( validators = dict( pid=validators.Int(),
								  signal=validators.Int() ) )
	def killproc( self, machine, pid, signal=None ):

		c = cluster.Cluster()
		m = c.getMachine( machine )
		p = m.getProc( pid )
		u = p.user()

		if not identity.in_group( "admin" ) and \
		   util.getServerUsername() != u.name:
			return self.error( "You can't kill other people's processes" )

		m.killProc( p, signal )
		raise redirect( "procs", user=u.name )

	@identity.require(identity.not_anonymous())
	@expose()
	@util.unicodeToStr
	def doStart( self, user, mode, group=None, machine=None, layout=None,
				 restrict=True ):

		c = cluster.Cluster()
		user = c.getUser( user )
		kw = {}
		util.clearDebugOutput()

		# Saved layout mode
		if mode == "layout":
			rec = self.getLayout( layout )
			if not rec:
				return self.error( "Couldn't find saved layout '%s' "
								   "in the database" % layout )
			task = async_task.AsyncTask( 0, user.startFromXML,
										 StringIO( rec.xmldata ) )

		# Single and group modes
		else:
			if mode == "single":
				machines = [c.getMachine( machine )]
			else:
				machines = None

			if mode == "group":

				# If the selected group is all machines, force the group
				# to be None. This will replicate the behavior of
				# 'cluster_control.py start all'
				if group == "(use all machines)":
					group = None
					
				kw[ "group" ] = group
				if restrict:
					kw[ "tags" ] = True

			task = async_task.AsyncTask( 0, user.start, machines, **kw )

		# Delete old pref
		for rec in model.ccStartPrefs.select(
			model.ccStartPrefs.q.userID == util.getSessionUID() ):
			rec.destroySelf()

		# Insert new pref
		if mode == "single":
			model.ccStartPrefs( user = util.getSessionUser(),
								mode = "single",
								arg = machine,
								useTags = bool( restrict ) )
		elif mode == "group":
			model.ccStartPrefs( user = util.getSessionUser(),
								mode = "group",
								arg = group,
								useTags = bool( restrict ) )

		elif mode == "layout":
			model.ccStartPrefs( user = util.getSessionUser(),
								mode = "layout",
								arg = layout,
								useTags = False )

		# Block until we get the layout out of the async task
		try:
			layout = task.waitForState( "layout" )[1][:]
		except task.TerminateException:
			return self.error( "Couldn't get layout" )

		# Tag each process as "running"
		for i in xrange( len( layout ) ):
			layout[i] = layout[i] + ("running",)

 		return self.toggle( "start", task.id, layout, user )

	@identity.require(identity.not_anonymous())
	@expose()
	def restart( self, user ):
		c = cluster.Cluster()
		user = c.getUser( user )
		layout = user.getLayout()
		self.appendToLayout( layout, "registered" )
		task = async_task.AsyncTask( 0, user.restart )
		return self.toggle( "restart", task.id, layout, user )

	@identity.require(identity.not_anonymous())
	@expose()
	def stop( self, user ):
		c = cluster.Cluster()
		user = c.getUser( user )
		layout = user.getLayout()
		self.appendToLayout( layout, "registered" )
		task = async_task.AsyncTask( 0, user.smartStop )
		return self.toggle( "stop", task.id, layout, user )

	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.toggle" )
	def toggle( self, action, id, layout, user ):

		# Figure out pnames set
		pnames = set()
		for _, pname, _, _ in layout:
			pnames.add( pname )
		pnames = list( pnames )
		pnames.sort( cluster.Process.cmpByName )

		return dict( action = action, layout = layout,
					 pnames = pnames, user = user, id = id )

	@identity.require(identity.not_anonymous())
	@ajax.expose
	def verifyEnv( self, user, type, value ):

		c = cluster.Cluster()
		util.clearDebugOutput()
		ms = []

		if type == "machine":
			ms.append( c.getMachine( value ) )

		elif type == "group":
			groups = c.getGroups()
			if value == "(use all machines)":
				ms = c.getMachines()
			elif groups.has_key( value ):
				ms = groups[ value ]
			else:
				raise ajax.Error( "Unknown group: %s" % value,
								  util.getDebugErrors() )

		elif type == "layout":
			layout = self.getLayout( value )
			mnames = [m for (m,p) in cluster.User.parseXMLLayout( layout.xmldata )]

			ms = []; missing = False
			for mname in mnames:
				ms.append( c.getMachine( mname ) )
				if not ms[-1]:
					log.error( "Layout refers to unknown machine %s", mname )
					missing = True

			if missing:
				raise ajax.Error( "Layout refers to unknown machines",
								  util.getDebugErrors() )

		user = c.getUser( user, random.choice( ms ), refreshEnv = True )

		if user.verifyEnvSync( ms ):
			return dict( mfroot = user.mfroot, bwrespath = user.bwrespath )

		else:
			raise ajax.Error(
				"Inconsistent environment settings across target machines",
				util.getDebugErrors() )


	@identity.require(identity.not_anonymous())
	@expose( template="cluster_control.templates.coredumps" )
	def coredumps( self, user = None ):

		if not user:
			user = util.getServerUsername()

		c = cluster.Cluster.get( user = user )
		user = c.getUser( user, checkCoreDumps = True )
		coredumps = sorted( user.coredumps, key = lambda x: x[2] )
		return dict( user = user, coredumps = coredumps )


	# --------------------------------------------------------------------------
	# Section: Saved XML layouts
	# --------------------------------------------------------------------------

	@util.unicodeToStr
	def getLayout( self, name ):
		recs = list( model.ccSavedLayouts.select( sqlobject.AND(
			model.ccSavedLayouts.q.userID == util.getSessionUID(),
			model.ccSavedLayouts.q.name == name ) ) )
		if len( recs ) == 1:
			return recs[0]
		elif len( recs ) == 0:
			return None
		else:
			log.critical( "Multiple saved layouts called '%s' exist for %s",
						  name, util.getSessionUsername() )

	@identity.require( identity.not_anonymous() )
	@ajax.expose
	def saveLayout( self, user, name ):

		c = cluster.Cluster()
		user = c.getUser( user )
		stream = StringIO()
		user.saveToXML( stream )

		# Delete any existing query with the same name
		old = self.getLayout( name )
		if old: old.destroySelf()

		model.ccSavedLayouts( user = util.getSessionUser(),
							  name = name,
							  serveruser = user.name,
							  xmldata = stream.getvalue() )

		return "Server layout saved successfully"

	@identity.require( identity.not_anonymous() )
	@expose()
	def deleteLayout( self, name ):
		rec = self.getLayout( name )
		if not rec:
			return self.error( "Can't delete non-existant layout '%s'" % name )
		else:
			rec.destroySelf()
			raise redirect( "layouts" )

	@identity.require(identity.not_anonymous())
	@expose( template="web_console.cluster_control.templates.layouts" )
	def layouts( self ):
		recs = model.ccSavedLayouts.select(
			model.ccSavedLayouts.q.userID == util.getSessionUID() )

		# Convert each XML layout into a mapping of process counts
		layouts = []
		pnames = set()
		for rec in recs:
			counts = {}
			layout = cluster.User.parseXMLLayout( rec.xmldata )
			for mname, pname in layout:
				pnames.add( pname )
				if counts.has_key( pname ):
					counts[ pname ] += 1
				else:
					counts[ pname ] = 1
			layouts.append( counts )

		# Sort pnames by pre-arranged ordering here
		pnames = list( pnames )
		pnames.sort( cluster.Process.cmpByName )

		return dict( recs = recs, layouts = layouts, pnames = pnames )

	@identity.require(identity.not_anonymous())
	@expose( template="web_console.common.templates.error" )
	def error( self, msg ):
		debugmsgs = util.getDebugErrors()
		return dict( msg = msg, debug = debugmsgs )
