import BigBang
import ResMgr

import ToolbarUIAdapter
reload( ToolbarUIAdapter )
import TerrainUIAdapter
reload( TerrainUIAdapter )
import ItemUIAdapter
reload( ItemUIAdapter )
import Actions
reload( Actions )

from BigBangDirector import bd
from BigBangDirector import oi
from ToolbarUIAdapter import *
from TerrainUIAdapter import *
from ItemUIAdapter import *
from Actions import *


"""This module routes user interface events from the Borland GUI through
to the c++ WorldEditor and python BigBangDirector"""


#--------------------------------------------------------------------------
#	Section - unimplemented methods
#--------------------------------------------------------------------------
def onButtonClick( name ):
	BigBang.addCommentaryMsg( "%s click not yet implemented" % name, 1 )

def onSliderAdjust( name, value, min, max ):
	BigBang.addCommentaryMsg( "%s adjust not yet implemented %f < %f < %f" % (name, min, value, max ), 1 )

def onCheckBoxToggle( name, value ):
	BigBang.addCommentaryMsg( name + " toggle not yet implemented. value = " + str( value ), 1 )

def onComboBoxSelect( name, selectionName ):
	BigBang.addCommentaryMsg( "%s select not yet implemented. selection = %s" % (name, selectionName), 1 )

def onEvent( event, value ):
	BigBang.addCommentaryMsg( "Generic event (%s, %s) not yet implemented" % (event, str(value) ), 1 )

def onBrowserItemSelect( name, filename ):
	BigBang.addCommentaryMsg( "%s browser item (%s) select not yet implemented" % (name, filename), 1 )

def onListItemSelect( name, index ):
	BigBang.addCommentaryMsg( "%s list item[%i] select not yet implemented" % (name, index), 1 )


# ------------------------------------------------------------------------------
# Section: Individual command methods
# ------------------------------------------------------------------------------

# ---- Far plane ----

def slrFarPlaneAdjust( value, min, max ):
	BigBang.farPlane( value )

def slrFarPlaneUpdate():
	return BigBang.farPlane()

def edtFarPlaneExit( value ):
	if value[-2:] == "cm":
		floatValue = float(value[:-2])/100.0
	elif value[-1] == "m":
		floatValue = float(value[:-1])
	else:
		floatValue = float(value)

	BigBang.farPlane( floatValue )

def edtFarPlaneUpdate():
	return "%.0fm" % (BigBang.farPlane(), )

# ---- Misc ----

def slrProjectCurrentTimeAdjust( value, min, max ):
	percent = (value-min) / (max-min) * 23.9
	BigBang.romp.setTime( percent )

# ---- Snaps ----

def edtMiscSnapsXExit( value ):
	if value[-2:] == "cm":
		floatValue = float(value[:-2])/100.0
	elif value[-1] == "m":
		floatValue = float(value[:-1])
	else:
		floatValue = float(value)

	ns = BigBang.getOptionVector3( "snaps/movement" )
	BigBang.setOptionVector3( "snaps/movement", ( floatValue, ns[1], ns[2] ) )

def edtMiscSnapsXUpdate():
	snaps = BigBang.getOptionVector3( "snaps/movement" )
	return "%0.1fm" % (snaps[0], )


def edtMiscSnapsYExit( value ):
	if value[-2:] == "cm":
		floatValue = float(value[:-2])/100.0
	elif value[-1] == "m":
		floatValue = float(value[:-1])
	else:
		floatValue = float(value)

	ns = BigBang.getOptionVector3( "snaps/movement" )
	BigBang.setOptionVector3( "snaps/movement", ( ns[0], floatValue, ns[2] ) )

def edtMiscSnapsYUpdate():
	snaps = BigBang.getOptionVector3( "snaps/movement" )
	return "%0.1fm" % (snaps[1], )


def edtMiscSnapsZExit( value ):
	if value[-2:] == "cm":
		floatValue = float(value[:-2])/100.0
	elif value[-1] == "m":
		floatValue = float(value[:-1])
	else:
		floatValue = float(value)

	ns = BigBang.getOptionVector3( "snaps/movement" )
	BigBang.setOptionVector3( "snaps/movement", ( ns[0], ns[1], floatValue ) )

def edtMiscSnapsZUpdate():
	snaps = BigBang.getOptionVector3( "snaps/movement" )
	return "%0.1fm" % (snaps[2], )


def pgcAllToolsTabSelect( value ):
	global bd

	if value == "tabTerrain":
		if ( bd != None ):
			bd.enterMode( "Terrain" )
	elif value == "tabTerrainImport":
		if ( bd != None ):
			bd.enterMode( "Height" )
	elif value in ("tabObject", "tabScene"):
		if ( bd != None ):
			bd.enterMode( "Object" )
	elif value == "tabProject":
		if ( bd != None ):
			bd.enterMode( "Project" )
	elif ( bd != None ):
		if ( bd.getMode() == "Project" ):
			bd.enterMode( "Object" ) # enter Object mode if in Project mode


def pgcObjectsTabSelect( value ):
	global oi
	if value != "tabObjectUal":
		if value == "tabObjectShell" or value == "tabPrefabs" or value == "tabObjectPrefabs":
			oi.setShellMode( 1 )
		else:
			oi.setShellMode( 0 )

# Set the active tab to bd.currentTab, if it's specified
def pgcAllToolsUpdate():
	global bd

	val = None

	if bd != None and hasattr( bd, "currentTab" ):
		val = bd.currentTab
		bd.currentTab = None

	return val


#--------------------------------------------------------------------------
#	Section - The Project tab
#--------------------------------------------------------------------------
def projectLock( commitMsg ):
	BigBang.projectLock( commitMsg )

def actProjectProgressExecute():
	BigBang.projectProgress()

def projectCommitChanges( commitMsg, keepLocks ):
	BigBang.projectCommit( commitMsg, keepLocks )

def projectDiscardChanges( commitMsg, keepLocks ):
	BigBang.projectDiscard( commitMsg, keepLocks )

def projectUpdateSpace():
	BigBang.projectUpdateSpace()
	
def projectCalculateMap():
	BigBang.projectCalculateMap()

def projectExportMap():
	BigBang.projectExportMap()

def slrProjectMapBlendAdjust( value, min, max ):
	BigBang.projectMapAlpha( (value-min) / (max-min) )
		
def slrProjectMapBlendUpdate():
	return 1.0 + BigBang.projectMapAlpha() * 99.0

selectFilters = (
					( "All" , "" ),
					( "All Except Terrain and Shells" , "" ),
					# Raymond, simple modification to change the name
					# ( "Shells Only" , "" ),
					( "Shells + Contents" , "" ),
					( "All Lights" , "spotLight|ambientLight|directionalLight|omniLight|flare|pulseLight" ),
					( "Omni Lights" , "omniLight" ),
					( "Ambient Lights" , "ambientLight" ),
					( "Directional Lights" , "directionalLight" ),
					( "Pulse Lights" , "pulseLight" ),
					( "Spot Lights" , "spotLight" ),
					( "Models" , "model|speedtree" ),
					( "Trees" , "speedtree" ),
					( "Entities" , "entity" ),
					( "Clusters and Markers" , "marker|marker_cluster" ),
					( "Particles" , "particles" ),
					( "Waypoint Stations" , "station" ),
					( "Terrain" , "terrain" ),
					( "Sounds" , "sound" ),
					( "Water" , "water" ),
					( "Portals" , "portal" )
				)

def cmbSelectFilterKeys():
	return selectFilters

def cmbSelectFilterUpdate():
	return (BigBang.getOptionString( "tools/selectFilter" ), )

def setSelectionFilter( name ):
	filter = ""
	for item in selectFilters:
		if item[0] == name:
			filter = item[1]

	BigBang.setSelectionFilter( filter )

	if name == "Portals":
		BigBang.setNoSelectionFilter( "" )
	else:
		BigBang.setNoSelectionFilter( "portal" )

	if name == "All Except Terrain and Shells":
		BigBang.setNoSelectionFilter( "portal|terrain" )
		BigBang.setSelectShellsOnly( 2 )
	elif name == "Shells + Contents":
		BigBang.setSelectShellsOnly( 1 )
	elif name == "Models":
		BigBang.setSelectShellsOnly( 2 )
	else:
		BigBang.setSelectShellsOnly( 0 )
		
def cmbSelectFilterChange( value ):
	if(	BigBang.getOptionString( "tools/selectFilter" ) != value ):
		BigBang.addCommentaryMsg( "Selection Filter: %s" % value )
		BigBang.setOptionString( "tools/selectFilter", value )
	setSelectionFilter( value )
	BigBang.setToolMode( "Objects", )

def doSetSelectionFilter( item ):
	cmbSelectFilterChange( item.displayName )
	pass

def updateSelectionFilter( item ):
	if item.displayName == BigBang.getOptionString( "tools/selectFilter" ):
		return 1
	return 0

coordFilters = (
					( "World" , "" ),
					( "Local" , "" ),
					( "View" , "" ),
				)


def cmbCoordFilterKeys():
	return coordFilters

def cmbCoordFilterUpdate():
	return (BigBang.getOptionString( "tools/coordFilter" ), )

def cmbCoordFilterChange( value ):
	BigBang.addCommentaryMsg( "Reference Coordinate System: %s" % value )
	BigBang.setOptionString( "tools/coordFilter", value )

def actSaveCameraPositionExecute():
	dir = BigBang.getOptionString( "space/mru0" )
	dirDS = ResMgr.openSection( dir )
	if not dirDS:
		BigBang.addCommentaryMsg( "Unable to open local directory " + dir )
		return

	ds = dirDS["space.localsettings"]
	if ds == None:
		ds = dirDS.createSection( "space.localsettings" )

	if ds == None:
		BigBang.addCommentaryMsg( "Unable to create space.localsettings" )
		return


	m = BigBang.camera(0).view
	m.invert()
	ds.writeVector3( "startPosition", m.translation )
	ds.writeVector3( "startDirection", (m.roll, m.pitch, m.yaw) )
	ds.save()

	BigBang.addCommentaryMsg( "Camera position saved" )



def actSaveStartPositionExecute():
	dir = BigBang.getOptionString( "space/mru0" )
	dirDS = ResMgr.openSection( dir )
	if not dirDS:
		BigBang.addCommentaryMsg( "Unable to open local directory " + dir )
		return

	ds = dirDS["space.settings"]
	if ds == None:
		ds = dirDS.createSection( "space.settings" )

	if ds == None:
		BigBang.addCommentaryMsg( "Unable to create space.settings" )
		return


	m = BigBang.camera().view
	m.invert()

	ds.writeVector3( "startPosition", m.translation )
	ds.writeVector3( "startDirection", (m.roll, m.pitch, m.yaw) )
	ds.save()

	BigBang.addCommentaryMsg( "Start position set to camera position" )

def canSavePrefab():
	return bd.itemTool.functor.script.selection.size;

def savePrefab( name ):
	message = BigBang.saveChunkPrefab( bd.itemTool.functor.script.selection, name )
	if message != None:
		BigBang.addCommentaryMsg( message, 0 )



#--------------------------------------------------------------------------
# Section - ItemUIAdapter
#--------------------------------------------------------------------------

def brwObjectItemSelect( self, value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	cmbSelectFilterChange( "All" )

def brwObjectModelItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	filter = oi.setObjectTab( "Model" )
	oi.setShellMode( 0 )
	if filter == "":
		filter = "Models"
	cmbSelectFilterChange( filter )

def brwObjectShellItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	filter = oi.setObjectTab( "Shell" )
	oi.setShellMode( 1 )
	if filter == "":
# Raymond, simple modification to change the name	
		# filter = "Shells Only"
		filter = "Shells + Contents"
	cmbSelectFilterChange( filter )

def brwObjectPrefabsItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	filter = oi.setObjectTab( "Prefabs" )
	if filter == "":
		filter = "All"
	cmbSelectFilterChange( filter )

def brwObjectEntityItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	filter = oi.setObjectTab( "Entity" )
	if filter == "":
		filter = "Entities"
	cmbSelectFilterChange( filter )

def brwObjectLightsItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	filter = oi.setObjectTab( "Lights" )
	if filter == "":
		filter = "All Lights"
	cmbSelectFilterChange( filter )

def brwObjectParticlesItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	filter = oi.setObjectTab( "Particles" )
	if filter == "":
		filter = "Particles"
	cmbSelectFilterChange( filter )

def brwObjectMiscItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	filter = oi.setObjectTab( "Misc" )
	if filter == "":
		filter = "All Except Terrain and Shells"
	cmbSelectFilterChange( filter )

def brwObjectEntityBaseClassesItemSelect( value ):
	global oi
	oi.setBrowsePath( value )
	oi.showBrowse()
	cmbSelectFilterChange( "Entities" )


#--------------------------------------------------------------------------
# Section - UAL related methods
#--------------------------------------------------------------------------

def ualSelectFilterChange( value ):
	if(	BigBang.getOptionString( "tools/selectFilter" ) != value ):
		BigBang.addCommentaryMsg( "Selection Filter: %s" % value )
		BigBang.setOptionString( "tools/selectFilter", value )
	setSelectionFilter( value )

def brwObjectUalItemSelect( value ):
	global oi
	if value.find( "/shells" ) == -1 and value[:7] != "shells/":
		oi.setShellMode( 0 )
		ualSelectFilterChange( "All Except Terrain and Shells" )
	else:
		oi.setShellMode( 1 )
		ualSelectFilterChange( "Shells + Contents" )

	oi.setBrowsePath( value )
	oi.showBrowse()

def brwObjectItemAdd( ):
	bd.itemTool.functor.script.addChunkItem()

def contextMenuGetItems( type, path ):
	if path[-4:] == ".xml" and path.find( "particles" ) != -1:
		return [ ( 1, "Edit in Particle Editor..." ) ]
	elif path[-6:] == ".model":
		return [ ( 2, "Edit in Model Editor..." ) ]
	return []

def contextMenuHandleResult( type, path, command ):
	if command == 1:
		BigBang.launchTool( "particleeditor", "-o" + path )
	elif command == 2:
		BigBang.launchTool( "modeleditor", "-o" + path )

# set selection filter at launch
setSelectionFilter( BigBang.getOptionString( "tools/selectFilter" ) )
