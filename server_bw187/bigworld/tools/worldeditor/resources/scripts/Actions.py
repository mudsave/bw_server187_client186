import BigBang
from BigBangDirector import bd

# ------------------------------------------------------------------------------
# Section: Default Action execution
# ------------------------------------------------------------------------------

class OptionSet:
	"""This class is used to implement action handlers that sets an option based
	on a simple value."""

	def __init__( self, path, value ):
		self.path = path
		self.value = value

	def __call__( self ):
		BigBang.setOption( self.path, self.value )

class OptionSetPlus( OptionSet ):
	"""This class is used to implement action handlers that sets an option based
	on a simple value."""

	def __init__( self, path, value, postFunction ):
		OptionSet.__init__( self, path, value )
		self.postFunction = postFunction

	def __call__( self ):
		OptionSet.__call__( self )
		self.postFunction( self.value )

class OptionToggle:
	"This class is used to implement action handlers that toggles an option."

	def __init__( self, path, postFunction = None ):
		self.path = path
		self.postFunction = postFunction

	def __call__( self ):
		oldValue = BigBang.getOptionInt( self.path )
		newValue = not oldValue
		BigBang.setOption( self.path, newValue )
		if self.postFunction:
			self.postFunction( newValue )
			

class OptionSetFunction:
	"""This class is used to implement action handlers that sets an option based
	on a simple function."""

	def __init__( self, path, fn ):
		self.path = path
		self.fn = fn

	def __call__( self ):
		BigBang.setOption( self.path, self.fn() )

def defaultActionExecute( name ):
	"""This function is the default handler for executing actions."""
	BigBang.addCommentaryMsg( "%s execute not yet implemented" % name )

# ------------------------------------------------------------------------------
# Section: Default Action enablers
# ------------------------------------------------------------------------------

intReader = BigBang.getOptionInt
floatReader = BigBang.getOptionFloat
stringReader = BigBang.getOptionString
vector3Reader = BigBang.getOptionVector3
vector4Reader = BigBang.getOptionVector4

class OptionCheck:
	def __init__( self, path, value, reader = intReader ):
		self.path = path
		self.value = value
		self.readValue = reader

	def __call__( self ):
		return (1, self.readValue( self.path ) == self.value )

def defaultActionUpdate( name ):
	return (1, 0)
	

# Mark menu items for unimplemented features as disabled
	
def actPreferencesUpdate():
	return (0, 0)
	
def actNewProjectUpdate():
	return (0, 0)
	
def actOpenProjectUpdate():
	return (0, 0)
	
def actZoomExtentsUpdate():
	return (0, 0)
	
def actSaveProjectAsUpdate():
	return (0, 0)
	
def actUndoHistoryUpdate():
	return (0, 0)

def actAnimateDayNight():
	return (0, 0)

# ------------------------------------------------------------------------------
# Section: Specialised Action execution
# ------------------------------------------------------------------------------
def actSaveChunkTemplateExecute():
	if bd.itemTool.functor.script.selection.size:
		BigBang.saveChunkTemplate( bd.itemTool.functor.script.selection )
	else:
		BigBang.addCommentaryMsg( "Nothing selected" )
		
def actSelectAllExecute():
	group = BigBang.selectAll()
	if ( group != None ):
		bd.itemTool.functor.script.selection.rem( bd.itemTool.functor.script.selection )
		bd.itemTool.functor.script.selection.add( group )
		bd.itemTool.functor.script.selUpdate()

def actDeselectAllExecute():
	if bd.itemTool.functor.script.selection.size:
		bd.itemTool.functor.script.selection.rem( bd.itemTool.functor.script.selection )
		bd.itemTool.functor.script.selUpdate()
		
def actImportExecute():
	BigBang.importDataGUI()

def actExportExecute():
	BigBang.exportDataGUI()



# ------------------------------------------------------------------------------
# Section: Specialised Action enablers
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Section: Radio button
# ------------------------------------------------------------------------------

def updateCamera( value ):
	c = BigBang.camera()
	c.speed = BigBang.getOptionFloat( "camera/speed/" + value )
	c.turboSpeed = BigBang.getOptionFloat( "camera/speed/" + value + "/turbo" )

def onUpdateSnaps( value ):
	bd.updateItemSnaps()
# below method for borland big bang where only two buttons
#	bd.toggleItemSnaps()	

def onUpdateCoordMode( value ):
	bd.updateCoordMode()

def onOrthoMode( self ):
	if BigBang.getOptionInt( "camera/ortho" == 1 ):
		BigBang.changeToCamera(1)
	else:
		BigBang.changeToCamera(0)
		
def toggleBSP( self ):
	bd.showBSPMsg( BigBang.getOptionInt( "drawBSP" ) )


radioButtons = (
	("actMoveSlow",					"camera/speed", "Slow",		updateCamera ),
	("actMoveMedium",				"camera/speed", "Medium",	updateCamera ),
	("actMoveFast",					"camera/speed", "Fast",		updateCamera ),
	("actMoveSuperFast",			"camera/speed", "SuperFast", updateCamera ),

	("actLightingStd",				"render/lighting", 0, bd.lightingModeMsg ),
	("actLightingDynamic",			"render/lighting", 1, bd.lightingModeMsg ),
	("actLightingSpecular",			"render/lighting", 2, bd.lightingModeMsg ),

	("actDrawBSPNormal",			"drawBSP", 0 ),
	("actDrawBSPCustom",			"drawBSP", 1 ),
	("actDrawBSPAll",				"drawBSP", 2 ),

	("actXZSnap",					"snaps/itemSnapMode", 0, onUpdateSnaps ),
	("actTerrainSnaps",				"snaps/itemSnapMode", 1, onUpdateSnaps ),
	("actObstacleSnap",				"snaps/itemSnapMode", 2, onUpdateSnaps ),
	
	("actCoordWorld",				"coordMode/coordMode", 0, onUpdateCoordMode ),
	("actCoordObject",				"coordMode/coordMode", 1, onUpdateCoordMode ),
	("actCoordView",				"coordMode/coordMode", 2, onUpdateCoordMode ),
)

toggleButtons = (
	("actDrawScenery",					"render/scenery", 0 ),
	("actDrawSceneryWireframe",			"render/scenery/wireFrame" ),
	("actDrawSceneryParticle",			"render/scenery/particle" ),
	("actDrawSceneryBSP",				"drawBSP", toggleBSP ),
	("actDrawDebugBB",					"debugBB" ),

	("actDrawTerrain",					"render/terrain", 0 ),
	("actDrawTerrainWireframe",			"render/terrain/wireFrame" ),
	# TODO: It would probably be better to change the action's name.
	("actRenderTerrainWireframe",		"render/terrain/wireFrame" ),

	("actDrawShells",					"render/shells" ),
	("actDrawShellNeighboursPortals",	"render/shells/gameVisibility" ),

	("actOrthoMode",					"camera/ortho", onOrthoMode ),

	("actToggleSnaps",					"snaps/xyzEnabled", bd.objectSnapMsg ),

	("actDrawEnvironment",				"render/environment" ),
	("actDrawSky",						"render/environment/drawSky" ),
	("actDrawClouds",					"render/environment/drawClouds" ),
	("actDrawWater",					"render/environment/drawWater" ),
	("actDrawWaterRefraction",	"render/environment/drawWater/refraction"),
	("actDrawWaterReflection",	"render/environment/drawWater/reflection"),
	("actDrawWaterSimulation",	"render/environment/drawWater/simulation"),
	("actDrawSunAndMoon",				"render/environment/drawSunAndMoon" ),
	("actDrawStars",					"render/environment/drawStars" ),
	("actDrawFog",						"render/environment/drawFog" ),
	("actDrawDetailObjects",			"render/environment/drawDetailObjects" ),
	("actDrawBloom",					"render/environment/drawBloom" ),
	("actDrawShimmer",					"render/environment/drawShimmer" ),
	
	
	
	("actDrawMisc",						"render/misc" ),
	("actDrawHeavenAndEarth",			"render/misc/drawHeavenAndEarth" ),
	("actShadeReadOnlyAreas",			"render/misc/shadeReadOnlyAreas" ),
	("actDrawPatrolGraphs",				"render/misc/drawPatrolGraphs" ),
	
	("actDrawLights",					"render/lights" ),
	("actDrawLightStatic",				"render/lights/drawStatic", bd.lightingModeMsg ),
	("actDrawLightDynamic",				"render/lights/drawDynamic", bd.lightingModeMsg ),
	("actDrawLightSpecular",			"render/lights/drawSpecular", bd.lightingModeMsg ),
	
	("actProjectOverlayLocks",			"render/misc/drawOverlayLocks" ),
	
	("actEnableDynamicUpdating",		"enableDynamicUpdating" ),
	
	("actDragOnSelect",					"dragOnSelect", bd.dragOnSelectMsg ),
	
	("actShowDate",					"messages/showDate",		0 ),
	("actShowTime",					"messages/showTime",		0 ),
	("actShowPriority",				"messages/showPriority",	0 ),
	("actErrorMsgs",				"messages/errorMsgs",		0 ),
	("actWarningMsgs",				"messages/warningMsgs",		0 ),
	("actNoticeMsgs",				"messages/noticeMsgs",		0 ),
	("actInfoMsgs",					"messages/infoMsgs",		0 ),
	("actAssetMsgs",				"messages/assetMsgs",		0 ),
)

for entry in radioButtons:
	value = entry[2]

	if len(entry) == 3:
		locals()[ entry[0] + "Execute" ]	= OptionSet( entry[1], value )
	else:
		locals()[ entry[0] + "Execute" ]	= OptionSetPlus( entry[1], value, entry[3] )

	if isinstance( value, int ):
		reader = intReader
	elif isinstance( value, float ):
		reader = floatReader
	elif isinstance( value, str ):
		reader = stringReader
	elif isinstance( value, tuple ):
		if len(value) == 3:
			reader = vector3Reader
		else:
			reader = vector4Reader
	locals()[ entry[0] + "Update" ]		= OptionCheck( entry[1], value, reader )

for entry in toggleButtons:
	if len(entry) == 3:
		locals()[ entry[0] + "Execute" ] = OptionToggle( entry[1], entry[2] )
	else:
		locals()[ entry[0] + "Execute" ] = OptionToggle( entry[1] )

	locals()[ entry[0] + "Update"  ] = OptionCheck( entry[1], len(entry) != 4 )

# ------------------------------------------------------------------------------
# Section: Action list - description of actions
# ------------------------------------------------------------------------------

# actAnimateDayNight
# actAnimateScene
# actConstrainToBox
# actContinuousUpdate
# actDebugMessages
# actDelete
# actDeselectAll
# actDeselectRoot
# actDrawGradientSkyDome
# actDrawSky
# actDrawStars
# actDrawSunAndMoon
# actExit
# actFogEnabled
# actForceDraw
# actFreeSnaps
# actGameAnimationSpeed
# actHelp
# actKeyboard
# actLightAmbient
# actLightAmbientDirectional
# actLightAmbientOnly
# actLightDirectional
# actLightEnabled
# actLightGame
# actLightOmni
# actLightsFlareColourize
# actLightSpot
# actMiscSnapsAlignmentAbsolute
# actMiscSnapsAlignmentRelative
# actMiscSnapsAlignmentRelativeToObjectWorld
# actMoveFast
# actMoveMedium
# actMoveSlow
# actMoveSuperFast
# actNewProject
# actOpenProject
# actOrthoMode
# actPreferences
# actProjectEnvironmentSave
# actRedo
# actRenderSceneryHide
# actRenderSceneryShow
# actRenderSceneryShowRootOnly
# actRenderScenerySolid
# actRenderSceneryWireframe
# actRenderShellModelSolid
# actRenderShellModelWireframe
# actRenderShellNeighboursDirectOnly
# actRenderShellNeighboursHideAll
# actRenderShellNeighboursHideContents
# actRenderShellNeighboursIsolated
# actRenderShellNeighboursShowAll
# actRenderShellPortals
# actRenderTerrainHide
# actRenderTerrainShow
# actRenderTerrainSolid
# actRenderTerrainWireframe
# actRootFollowsCamera
# actSaveProject
# actSaveProjectAs
# actSceneryDefaults
# actShellDefaults
# actShellSnaps
# actShowAreaOfEffect
# actShowLightModels
# actSnapshot
# actTerrainDefaults
# actToggleSnaps
# actUndo
# actUndoHistory
# actUserDefaults
# actZoomExtents
