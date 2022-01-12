import BigBang
from BigBangDirector import bd
import Personality
import ResMgr
from os import startfile

def doQuickSave( item ):
	Personality.preQuickSave()
	BigBang.quickSave()

def doFullSave( item ):
	Personality.preFullSave()
	BigBang.save()

def doImport( item ):
	BigBang.importDataGUI()
	
def doExport( item ):
	BigBang.exportDataGUI()
	
def doUndo( item ):
	what = BigBang.undo(0)
	if what:
		BigBang.addCommentaryMsg( "Undoing: " + what )
	BigBang.undo()

	bd.itemTool.functor.script.selUpdate()

def doRedo( item ):
	what = BigBang.redo(0)
	if what:
		BigBang.addCommentaryMsg( "Redoing: " + what )
	BigBang.redo()

	bd.itemTool.functor.script.selUpdate()

def doSelectAll( item ):
	group = BigBang.selectAll()
	if ( group != None ):
		bd.itemTool.functor.script.selection.rem( bd.itemTool.functor.script.selection )
		bd.itemTool.functor.script.selection.add( group )
		bd.itemTool.functor.script.selUpdate()

def doDeselectAll( item ):
	if bd.itemTool.functor.script.selection.size:
		bd.itemTool.functor.script.selection.rem( bd.itemTool.functor.script.selection )
		bd.itemTool.functor.script.selUpdate()

def doSaveChunkTemplate( item ):
	if bd.itemTool.functor.script.selection.size:
		BigBang.saveChunkTemplate( bd.itemTool.functor.script.selection )
	else:
		BigBang.addCommentaryMsg( "Nothing selected" )

def doSaveCameraPosition( item ):
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

def doSaveStartPosition( item ):
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

def onShowOrthoMode( item ):
		BigBang.setOptionInt( "camera/ortho", 1 )
		BigBang.changeToCamera(1)

def onHideOrthoMode( item ):
		BigBang.setOptionInt( "camera/ortho", 0 )
		BigBang.changeToCamera(0)

def updateCamera():
	value = BigBang.getOptionString( "camera/speed" )
	c = BigBang.camera()
	c.speed = BigBang.getOptionFloat( "camera/speed/" + value )
	c.turboSpeed = BigBang.getOptionFloat( "camera/speed/" + value + "/turbo" )

def doSlowCamera( item ):
	BigBang.setOptionString( "camera/speed", "Slow" );
	updateCamera()

def doMediumCamera( item ):
	BigBang.setOptionString( "camera/speed", "Medium" );
	updateCamera()

def doFastCamera( item ):
	BigBang.setOptionString( "camera/speed", "Fast" );
	updateCamera()

def doSuperFastCamera( item ):
	BigBang.setOptionString( "camera/speed", "SuperFast" );
	updateCamera()
	
def normalMode( item ):
	BigBang.setOptionInt( "render/chunk/vizMode", 0 )
	bd.enterChunkVizMode()

def boundaryBox( item ):
	BigBang.setOptionInt( "render/chunk/vizMode", 1 )
	bd.enterChunkVizMode()

def heightMap( item ):
	BigBang.setOptionInt( "render/chunk/vizMode", 2 )
	bd.enterChunkVizMode()

def meshMode( item ):
	BigBang.setOptionInt( "render/chunk/vizMode", 3 )
	bd.enterChunkVizMode()

def doSnapFreePositioning( item ):
	BigBang.setOptionInt( "snaps/itemSnapMode", 0 )
	bd.updateItemSnaps()

def doSnapTerrainLock( item ):
	BigBang.setOptionInt( "snaps/itemSnapMode", 1 )
	bd.updateItemSnaps()

def doSnapObstacleLock( item ):
	BigBang.setOptionInt( "snaps/itemSnapMode", 2 )
	bd.updateItemSnaps()


#
#  Panels functions
#  See getContentID in mainframe.cpp for info on the Panel/Tool IDs
#

#  setting the current tool mode
def doToolModeObject( item ):
	BigBang.setToolMode( "Objects" )
	
def doToolModeTerrainTexture( item ):
	BigBang.setToolMode( "TerrainTexture" )
	
def doToolModeTerrainHeight( item ):
	BigBang.setToolMode( "TerrainHeight" )
	
def doToolModeTerrainFilter( item ):
	BigBang.setToolMode( "TerrainFilter" )
	
def doToolModeTerrainMesh( item ):
	BigBang.setToolMode( "TerrainMesh" )
	
def doToolModeTerrainImpExp( item ):
	BigBang.setToolMode( "TerrainImpExp" )
	
def doToolModeProject( item ):
	BigBang.setToolMode( "Project" )

#  show panels

def doShowTools( item ):
	BigBang.showPanel( "Tools", 1 )

def doShowToolObject( item ):
	BigBang.showPanel( "Objects", 1 )

def doShowToolTerrainTexture( item ):
	BigBang.showPanel( "TerrainTexture", 1 )

def doShowToolTerrainHeight( item ):
	BigBang.showPanel( "TerrainHeight", 1 )

def doShowToolTerrainFilter( item ):
	BigBang.showPanel( "TerrainFilter", 1 )

def doShowToolTerrainMesh( item ):
	BigBang.showPanel( "TerrainMesh", 1 )

def doShowToolProject( item ):
	BigBang.showPanel( "Project", 1 )

#  show/hide other panels
def doShowPanelUAL( item ):
	BigBang.showPanel( "UAL", 1 )

def doShowPanelProperties( item ):
	BigBang.showPanel( "Properties", 1 )

def doShowPanelOptionsGeneral( item ):
	BigBang.showPanel( "Options", 1 )

def doShowPanelOptionsWeather( item ):
	BigBang.showPanel( "Weather", 1 )

def doShowPanelOptionsEnvironment( item ):
	BigBang.showPanel( "Environment", 1 )

def doShowPanelHistogram( item ):
	BigBang.showPanel( "Histogram", 1 )

def doShowPanelMessages( item ):
	BigBang.showPanel( "Messages", 1 )

def doRequestFeature( item ):
	startfile( "mailto:support@bigworldtech.com?subject=WorldEditor  %2D Feature Request %2F Bug Report" )