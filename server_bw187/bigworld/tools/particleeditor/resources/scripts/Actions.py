import ParticleEditorDirector
import ParticleEditor
import BigBang


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
		oldValue = ParticleEditor.opts.readInt( self.path )
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
	#ParticleEditor.addCommentaryMsg( "%s execute not yet implemented" % name )
	pass

# ------------------------------------------------------------------------------
# Section: Default Action enablers
# ------------------------------------------------------------------------------

intReader     = ParticleEditor.opts.readInt
floatReader   = ParticleEditor.opts.readFloat
stringReader  = ParticleEditor.opts.readString
vector3Reader = ParticleEditor.opts.readVector3
vector4Reader = ParticleEditor.opts.readVector4

class OptionCheck:
	def __init__( self, path, value, reader = intReader ):
		self.path = path
		self.value = value
		self.readValue = reader

	def __call__( self ):
		return (1, self.readValue( self.path ) == self.value )
		
class OptionIndex:
	def __init__( self, test ):
		self.test = test

	def __call__( self ):
		return (1, self.test())

# ------------------------------------------------------------------------------
# Section: Control Functionality
# ------------------------------------------------------------------------------

def updateCamera( value ):
	pass
	
def shadowIndex():
	enabled = BigBang.getOptionInt( "settings/showShadowing", 1 )
	quality = BigBang.getOptionInt( "renderer/shadows/quality", 0 )
	if enabled:
		return quality + 1
	else:
		return 0
		
def enableShadowing( value ):
	BigBang.setOptionInt( "settings/showShadowing", 1 )
	
def bkgIndex():
	terrain = BigBang.getOptionInt( "settings/useTerrain", 1 )
	floor = BigBang.getOptionInt( "settings/useFloor", 0 )
	if terrain:
		return 2
	else:
		return floor
		
def disableTerrain( value ):
	BigBang.setOptionInt( "settings/useTerrain", 0 )
		
# ------------------------------------------------------------------------------
# Section: Control handler layout
# ------------------------------------------------------------------------------

radioButtons = (
	("actMoveSlow",       "camera/speed",  "Slow",       updateCamera ),
	("actMoveMedium",     "camera/speed",  "Medium",     updateCamera ),
	("actMoveFast",       "camera/speed",  "Fast",       updateCamera ),
	("actMoveSuperFast",  "camera/speed",  "SuperFast",  updateCamera ),
	
	("actGetShadowIndex", shadowIndex),
	
	("actShadowOff",         "settings/showShadowing",    0 ),
	("actShadowLowQuality",  "renderer/shadows/quality",  0,  enableShadowing ),
	("actShadowMedQuality",  "renderer/shadows/quality",  1,  enableShadowing ),
	("actShadowHighQuality", "renderer/shadows/quality",  2,  enableShadowing ),
	
	("actGetBkgIndex", bkgIndex),
	
	("actBkgTerrain",	"settings/useTerrain",	1 ),
	("actBkgFloor",		"settings/useFloor",	1,	disableTerrain),
	("actBkgNone",		"settings/useFloor",	0,	disableTerrain),
)

toggleButtons = (
	("actShowAxes",               "settings/showAxes",               0 ),
	("actCheckForSparkles",       "settings/checkForSparkles",       0 ),
	("actShowBloom",              "render/environment/drawBloom",    0 ),
	("actShowShimmer",            "render/environment/drawShimmer",  0 ),
	
	("actShowModel",              "settings/showModel",              0 ),
	("actShowWireframe",          "settings/showWireframe",          0 ),
	("actShowSkeleton",           "settings/showSkeleton",           0 ),
	("actShowShadowing",          "settings/showShadowing",          0 ),
	("actShowBsp",                "settings/showBsp",                0 ),
	("actShowBoundingBox",        "settings/showBoundingBox",        0 ),
	("actShowPortals",            "settings/showPortals",            0 ),
	("actGroundModel",            "settings/groundModel",            0 ),
	("actCentreModel",            "settings/centreModel",            0 ),
	
	("actShowFloor",              "settings/showFloor",              0 ),
	("actFloorUseTexture",        "settings/floorUseTexture",        0 ),
	("actFloorRecievesLighting",  "settings/floorRecievesLighting",  0 ),
	("actFloorMirrorObjects",     "settings/floorMirrorObjects",     0 ),
	
	("actUseCustomLighting",      "settings/useCustomLighting",      0 ),
)

# ------------------------------------------------------------------------------
# Section: Create control handlers
# ------------------------------------------------------------------------------

for entry in radioButtons:

	#A special case for returning an index
	if len(entry) == 2:
		locals()[ entry[0] + "Update" ] = OptionIndex( entry[1] )
	else:
	
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
	