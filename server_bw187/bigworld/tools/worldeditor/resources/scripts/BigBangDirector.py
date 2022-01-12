import BigBang
import keys
import GUI
import Locator
import View
import Functor
import ResMgr
from keys import *

bd = None
oi = None

class BigBangDirector:
	def __init__( self ):
		global bd
		bd = self
		self.bo = BigBang.opts
		self.objInfo = ObjInfo()
		global oi
		oi = self.objInfo
		self.modeName = "Object"
		self.terrainModeName = "TerrainTexture"
		self.itemSnapMode = 0
		self.nextTimeDoSelUpdate = 0
		self.currentTerrainFilter = -1
		self.rightMouseButtonDown = 0
		self.mouseMoved = 0
		self.eDown = 0
		self.qDown = 0
		self.avatarMode = 0
		self.modeStack = []

		BigBang.setNoSelectionFilter( "portal" )
		BigBang.setOptionInt( "render/chunk/vizMode", 0 );

	def onStart( self ):

		# create chunk viz
		self.chunkViz = View.TerrainChunkTextureToolView( "resources/maps/gizmo/square.dds" )
		self.chunkViz.numPerChunk = 1

		# create vertex viz
		self.vertexViz = View.TerrainChunkTextureToolView( "resources/maps/gizmo/vertices.dds" )
		self.vertexViz.numPerChunk = 25

		# create terrain mesh viz
		self.meshViz = View.TerrainChunkTextureToolView( "resources/maps/gizmo/quads.dds" )
		self.meshViz.numPerChunk = 25

		# create alpha tool
		self.alphaTool = BigBang.Tool()
		self.alphaTool.functor = Functor.HeightPoleAlphaFunctor()
		self.alphaTool.locator = Locator.TerrainToolLocator()
		self.alphaToolTextureView = View.TerrainTextureToolView( "resources/maps/gizmo/alphatool.tga" )
		self.alphaTool.addView( self.alphaToolTextureView, "stdView" )
		self.alphaTool.size = 30
		self.alphaTool.strength = 1000

		# create height filter tool
		self.heightFunctor = Functor.HeightPoleHeightFilterFunctor()
		self.heightFunctor.index = 7
		self.heightFunctor.strengthMod = 1
		self.heightFunctor.framerateMod = 1
		self.heightFunctor.constant = 1.0
		self.heightFunctor.falloff = 2

		self.setHeightFunctor = Functor.HeightPoleSetHeightFunctor()
		self.setHeightFunctor.height = 0
		self.setHeightFunctor.relative = 0
		
		self.heightView = View.TerrainTextureToolView( "resources/maps/gizmo/heighttool.tga" )
		self.setHeightView = View.TerrainTextureToolView( "resources/maps/gizmo/squareTool.dds" )

		self.heightTool = BigBang.Tool()
		self.heightTool.locator = Locator.TerrainToolLocator()
		self.heightTool.functor = Functor.TeeFunctor( self.heightFunctor, self.setHeightFunctor, KEY_LCONTROL )
		self.heightToolTextureView = View.TeeView( self.heightView, self.setHeightView, KEY_LCONTROL )
		self.heightTool.addView( self.heightToolTextureView, "stdView" )
		self.heightTool.size = 30

		# create general filter tool
		self.filterTool = BigBang.Tool()
		self.filterTool.locator = Locator.TerrainToolLocator()
		self.filterTool.functor = Functor.HeightPoleHeightFilterFunctor()
		self.filterTool.functor.strengthMod = 1
		self.filterTool.functor.constant = 0.0
		self.filterToolTextureView = View.TerrainTextureToolView( "resources/maps/gizmo/filtertool.tga" )
		self.filterTool.addView( self.filterToolTextureView, "stdView" )
		self.filterTool.size = 30

		# create a hole cutter
		self.holeTool = BigBang.Tool()		
		self.holeTool.locator = Locator.TerrainHoleToolLocator()
		view = View.TerrainTextureToolView( "resources/maps/gizmo/squareTool.dds" )
		view.showHoles = True
		self.holeTool.addView( view, "stdView" )
		self.holeTool.functor = Functor.HeightPoleHoleFunctor()
		self.holeTool.size = 30

		# create the item tool
		self.itemTool = BigBang.Tool()
		self.itemToolXZLocator = Locator.TerrainToolLocator()
		self.itemTool.locator = Locator.TerrainToolLocator()
		self.itemToolTextureView = View.TerrainTextureToolView( "resources/maps/gizmo/cross.dds" )
		self.itemToolModelView = View.ModelToolView( "resources/models/pointer.model" )
		self.itemToolPlaneView = View.TerrainTextureToolView( "resources/maps/gizmo/cross.dds" )
		self.itemTool.addView( self.itemToolTextureView, "stdView" )
		# This changes our locator to a ChunkItemLocator
		self.itemTool.functor = Functor.ScriptedFunctor( ChunkItemFunctor( self.itemTool, self.objInfo ) )
		# Setup the correct subLocator for the ChunkItemLocator
		self.itemTool.locator.subLocator = self.itemToolXZLocator
		self.itemTool.size = 1

		# Make the closed captions commentary viewer
		self.cc = GUI.ClosedCaptions( BigBang.getOptionInt( "consoles/numMessageLines" ) )
		self.cc.addAsView()
		self.cc.visible = 1

		if ( BigBang.getOptionInt( "tools/showChunkVisualisation" ) == 1 ):
			BigBang.setOptionInt( "render/chunk/vizMode", 1)
		self.enterChunkVizMode()

		self.enterMode( self.modeName, 1 )

		# initialise the mouse move camera
		# load up the start position from space.localsettings
		startPos = (0,1.85,0)
		startDir = (0,0,0)

		dir = BigBang.getOptionString( "space/mru0" )
		dirDS = ResMgr.openSection( dir )
		ds = dirDS["space.localsettings"]
		if ds != None:
			startPos = ds.readVector3( "startPosition", startPos )
			startDir = ds.readVector3( "startDirection", startDir )

		m = BigBang.camera(0).view
		m.setIdentity()
		m.setRotateYPR( (startDir[2], startDir[1], startDir[0]) )
		m.translation = startPos
		m.invert()
		BigBang.camera(0).view = m

		# select the camera as specified in the options
		BigBang.changeToCamera( BigBang.getOptionInt( "camera/ortho" ) )

		# read the initial item snap mode
		self.updateItemSnaps();

	def onStop( self ):
		self.selEditor = None
		BigBang.setCurrentEditors()
		
		# Remove the closed captions commentary viewer
		self.cc.visible = 0
		self.cc.delAsView()
		del self.cc

		# Remove options entries that are messy and transient
		BigBang.saveOptions()

		return 0

	def onPause( self ):
		self.cc.visible = 0
		self.cc.delAsView()

		if ( BigBang.tool() != None ):
			BigBang.popTool()

		pass

	def onResume( self, exitCode ):
		self.cc.addAsView()
		self.cc.visible = 1
		BigBang.addCommentaryMsg( "entered edit mode." )

		self.enterMode( self.modeName, 1 )
		
	lightNames = ( "Standard", "Dynamic", "Specular" )
	lockNames = ( "free", "terrain", "obstacle" )
	
	def dragOnSelectMsg( self, state ):
		if state == 1:
			BigBang.addCommentaryMsg( "Enabling Drag On Select" )
		else:
			BigBang.addCommentaryMsg( "Disabling Drag On Select")
			
	def itemSnapModeMsg( self, state ):
		BigBang.addCommentaryMsg( "Entering %s snap mode" % self.lockNames[state] )
		
	def lightingModeMsg( self, state ):
		BigBang.addCommentaryMsg( "Lighting Mode: %s" % self.lightNames[state] )
		
	def objectSnapMsg( self, state ):
		if state == 1:
			BigBang.addCommentaryMsg( "Enabling Object Snap Grid" )
		else:
			BigBang.addCommentaryMsg( "Disabling Object Snap Grid" )
			
	def showBSPMsg( self, state ):
		if state == 1:
			BigBang.addCommentaryMsg( "Drawing custom BSPs" )
		else:
			BigBang.addCommentaryMsg( "Normal draw mode")

	def ownKeyEvent( self, key, modifiers ):
		
		if key == KEY_F9:
			if self.modeName[:7] == "Terrain":
				t = BigBang.tool()
				f = t.functor
				t.functor = Functor.HeightPoleEcotypeFunctor()
				Functor.applyEcoytpeFunctor( t )
				t.functor = f
			else:
				BigBang.addCommentaryMsg( "F9 only works in terrain editing mode", 2 )
		
		elif key == KEY_B:
			if (modifiers & MODIFIER_CTRL) == 0:
				curr = BigBang.getOptionInt( "drawBSP" )
				curr = ( curr + 1 ) % 2
				BigBang.setOptionInt( "drawBSP", curr )
				self.showBSPMsg( curr )
		
		elif key == KEY_M:
			curr = BigBang.getOptionInt( "dragOnSelect" )
			curr = ( curr + 1 ) % 2
			BigBang.setOptionInt( "dragOnSelect", curr )
			self.dragOnSelectMsg( curr )
	
		elif key == KEY_F8:
			curr = BigBang.getOptionString( "tools/coordFilter" )
			if curr == "World":
				curr = "Local"	
			elif curr == "Local":
				curr = "View"
			elif curr == "View":
				curr = "World"
			BigBang.setOptionString( "tools/coordFilter", curr )	
			BigBang.addCommentaryMsg( "Reference Coordinate System: %s" % curr )
		
		elif key == KEY_L:
			if modifiers & MODIFIER_CTRL:
				curr = BigBang.getOptionInt( "render/lighting" )
				curr = ( curr + 1 ) % 3
				BigBang.setOptionInt( "render/lighting", curr )
				self.lightingModeMsg( curr )
				
		elif key == KEY_G:
			curr = BigBang.getOptionInt( "snaps/xyzEnabled" )
			curr = ( curr + 1 )% 2
			BigBang.setOptionInt( "snaps/xyzEnabled", curr )
			self.objectSnapMsg( curr )

		t = BigBang.tool()
		if t == self.alphaTool:
			if key == KEY_MIDDLEMOUSE:
				BigBang.setTerrainTexture()
			elif (modifiers & MODIFIER_CTRL) == 0:
				if key == KEY_1:
					BigBang.setTerrainChannel( 0 )
				elif key == KEY_2:
					BigBang.setTerrainChannel( 1 )
				elif key == KEY_3:
					BigBang.setTerrainChannel( 2 )
				elif key ==  KEY_4:
					BigBang.setTerrainChannel( 3 )

		sizeSection = ''
		strengthSection = ''
		minSizeSection = ''
		maxSizeSection = ''
		minStrengthSection = ''
		maxStrengthSection = ''
		if t == self.alphaTool:
			sizeSection = 'terrain/texture/size'
			minSizeSection = 'terrain/texture/minsizelimit'
			maxSizeSection = 'terrain/texture/maxsizelimit'
			strengthSection = 'terrain/texture/strength'
			minStrengthSection = 'terrain/texture/minstrengthlimit'
			maxStrengthSection = 'terrain/texture/maxstrengthlimit'
		elif t == self.heightTool:
			sizeSection = 'terrain/height/size'
			minSizeSection = 'terrain/height/minsizelimit'
			maxSizeSection = 'terrain/height/maxsizelimit'
			strengthSection = 'terrain/height/strength'
			minStrengthSection = 'terrain/height/minstrengthlimit'
			maxStrengthSection = 'terrain/height/maxstrengthlimit'
		elif t == self.filterTool:
			sizeSection = 'terrain/filter/size'
			minSizeSection = 'terrain/filter/minsizelimit'
			maxSizeSection = 'terrain/filter/maxsizelimit'
		elif t == self.holeTool:
			sizeSection = 'terrain/cutRepair/size'
			minSizeSection = 'terrain/cutRepair/minsizelimit'
			maxSizeSection = 'terrain/cutRepair/maxsizelimit'

		if sizeSection:
			size = BigBang.getOptionFloat( sizeSection )
			minSize = BigBang.getOptionFloat( minSizeSection )
			maxSize = BigBang.getOptionFloat( maxSizeSection )
			if key == KEY_RBRACKET:
				if not ( modifiers & MODIFIER_SHIFT ):
					size = size * 1.25 + 1
					if size > maxSize:
						size = maxSize
					t.size = size
					BigBang.setOptionFloat( sizeSection, size )
					BigBang.addCommentaryMsg( "Tool size %0.1f" % size )
			elif key == KEY_LBRACKET:
				if not ( modifiers & MODIFIER_SHIFT ):
					size = size * 0.8 - 1
					if size < minSize:
						size = minSize
					t.size = size
					BigBang.setOptionFloat( sizeSection, size )
					BigBang.addCommentaryMsg( "Tool size %0.1f" % size )
		if strengthSection:
			strength = BigBang.getOptionFloat( strengthSection )
			minStrength = BigBang.getOptionFloat( minStrengthSection )
			maxStrength = BigBang.getOptionFloat( maxStrengthSection )
			if key == KEY_RBRACKET and strength >= 0 or key == KEY_LBRACKET and strength < 0:
				if modifiers & MODIFIER_SHIFT:
					if strength >= 0:
						strength = strength * 1.25 + 1
					else:
						strength = strength * 1.25 - 1
					if strength > maxStrength:
						strength = maxStrength
					t.strength = strength
					BigBang.setOptionFloat( strengthSection, strength )
					BigBang.addCommentaryMsg( "Tool strength %0.1f" % strength )
			elif key == KEY_LBRACKET and strength >= 0 or key == KEY_RBRACKET and strength < 0:
				if modifiers & MODIFIER_SHIFT:
					if strength >= 0:
						strength = strength * 0.8 - 1
					else:
						strength = strength * 0.8 + 1
					if strength < minStrength:
						strength = minStrength
					t.strength = strength
					BigBang.setOptionFloat( strengthSection, strength )
					BigBang.addCommentaryMsg( "Tool strength %0.1f" % strength )

	def onRightMouse( self ):
		self.itemTool.functor.script.onRightMouse();
	def onKeyEvent( self, isDown, key, modifiers ):
		if not BigBang.cursorOverGraphicsWnd():
			return 0

		if key == KEY_RIGHTMOUSE:
			if ( not self.rightMouseButtonDown ) and isDown:
				self.rightMouseButtonDown = 1
				self.mouseMoved = 0
			elif self.rightMouseButtonDown and not isDown:
				self.rightMouseButtonDown = 0
				if not self.mouseMoved:
					self.onRightMouse()
		handled = 0
		if self.avatarMode and key == KEY_Q:
			self.qDown = isDown
			self.eDown = 0
			handled = 1
		if self.avatarMode and key == KEY_E:
			self.eDown = isDown
			self.qDown = 0
			handled = 1
		if not handled:
			handled = BigBang.camera().handleKeyEvent( isDown, key, modifiers )
		if not handled and isDown:
			handled = self.ownKeyEvent( key, modifiers )
		if not handled and BigBang.tool() != None:
			handled = BigBang.tool().handleKeyEvent( isDown, key, modifiers )
		return handled

	def handleWheelCameraSpeed( self, mz ):
		# look at the rotator for changing the camera speed
		c = BigBang.camera()
		currentValue = BigBang.getOptionString( "camera/speed" )
		speeds = ["Slow", "Medium", "Fast", "SuperFast"]

		iSpeed = 0
		if currentValue == speeds[1]:
			iSpeed = 1
		elif currentValue == speeds[2]:
			iSpeed = 2
		elif currentValue == speeds[3]:
			iSpeed = 3

		if mz > 0:
			iSpeed = iSpeed + 1
			if iSpeed > 3:
				iSpeed = 3
		elif mz < 0:
			iSpeed = iSpeed - 1
			if iSpeed < 0:
				iSpeed = 0

		value = speeds[iSpeed]

		handled = 0
		if value != currentValue:
			c = BigBang.camera()
			BigBang.setOptionString( "camera/speed", value )
			c.speed = BigBang.getOptionFloat( "camera/speed/" + value )
			c.turboSpeed = BigBang.getOptionFloat( "camera/speed/" + value + "/turbo" )
			handled = 1
			BigBang.addCommentaryMsg( "New camera speed %s" % value, 1 )

		return handled

	def onMouseEvent( self, mx, my, mz ):
		handled = 0

		if mx or my:
			self.mouseMoved = 1

		legacyCameraSpeedWithWheel = False
		if BigBang.isKeyDown( KEY_MOUSE1 ) and BigBang.getOptionInt( "input/legacyMouseWheel" ) != 0:
			# right mouse down, see if the camera speed should change
			# don't allow tool to adjust objects
			self.handleWheelCameraSpeed( mz )
			legacyCameraSpeedWithWheel = True
		elif BigBang.tool():
			handled = BigBang.tool().handleMouseEvent( mx, my, mz )

		if not handled:
			handled = BigBang.camera().handleMouseEvent( mx, my, mz )

		legacyRotateItemWithWheel = \
			( ( BigBang.isKeyDown( KEY_LSHIFT ) \
				or BigBang.isKeyDown( KEY_RSHIFT ) \
				or BigBang.getOptionInt( "input/legacyMouseWheel" ) != 0 ) \
			and self.itemTool.functor.script.selection.size )

		if not handled \
			and mz != 0 \
			and not legacyRotateItemWithWheel \
			and not legacyCameraSpeedWithWheel:
			# zoom using scroll wheel
			handled = 1
			view = BigBang.camera().view
			view.invert()
			mult = mz / 1200.0

			if BigBang.isCapsLockOn():
				mult = mult * BigBang.camera().turboSpeed
			else:
				mult = mult * BigBang.camera().speed
			
			forward = view.applyToAxis( 2 )
			
			view.translation = (
				view.translation[0] + forward[0] * mult,
				view.translation[1] + forward[1] * mult,
				view.translation[2] + forward[2] * mult )
			
			view.invert()
			BigBang.camera().view = view

		return handled

	def updateOptions():
		pass

	def updateState( self, dTime ):
		GUI.update( dTime )
		self.cc.update( dTime )
		BigBang.camera().update( dTime )
		if ( BigBang.tool() != None and not BigBang.tool().applying ):
			self.objInfo.overGizmo = BigBang.gizmoUpdate( BigBang.worldRay() )

		# update the BigBang
		BigBang.update( dTime )

		# update tool views
		base = dTime / 5
		self.alphaToolTextureView.rotation += base * (1 + (self.alphaTool.strength / 650))
		self.heightView.rotation += base * (1 + (self.heightTool.strength / 650))
		self.filterToolTextureView.rotation += base

		if self.nextTimeDoSelUpdate == 2:
			self.itemTool.functor.script.restoreOldSelection()
			self.nextTimeDoSelUpdate = 0
		elif self.nextTimeDoSelUpdate == 1:
			self.itemTool.functor.script.clearAndSaveSelection()
			self.nextTimeDoSelUpdate = 2

		if BigBang.isInPlayerPreviewMode() and not self.avatarMode:
			BigBang.addCommentaryMsg( "entered avatar walkthrough mode." )
			self.avatarMode = 1
		if self.avatarMode and not BigBang.isInPlayerPreviewMode():
			self.qDown = 0
			self.eDown = 0
			self.avatarMode = 0
		if self.avatarMode:
			value = BigBang.getOptionString( "camera/speed" )
			speed = 1
			if value == "Medium":
				speed = 2
			if value == "Fast":
				speed = 3
			if value == "SuperFast":
				speed = 4
			if self.qDown and BigBang.getOptionInt( "graphics/cameraHeight" ) - speed <= 2:
				BigBang.setOptionInt( "graphics/cameraHeight", 2 )
			if self.qDown and BigBang.getOptionInt( "graphics/cameraHeight" ) > 2:
				BigBang.setOptionInt( "graphics/cameraHeight", BigBang.getOptionInt( "graphics/cameraHeight" ) - speed )
			if self.eDown:
				BigBang.setOptionInt( "graphics/cameraHeight", BigBang.getOptionInt( "graphics/cameraHeight" ) + speed )

			BigBang.snapCameraToTerrain()

		bd.alphaTool.size = BigBang.getOptionFloat( "terrain/texture/size" )
		bd.alphaTool.strength = BigBang.getOptionFloat( "terrain/texture/strength" )

		bd.heightTool.size = BigBang.getOptionFloat( "terrain/height/size" )
		bd.heightTool.strength = BigBang.getOptionFloat( "terrain/height/strength" )
		bd.setHeightFunctor.height = BigBang.getOptionFloat( "terrain/height/height" )
		bd.setHeightFunctor.relative = BigBang.getOptionInt( "terrain/height/relative" )
		bd.heightFunctor.falloff = BigBang.getOptionInt( "terrain/height/brushFalloff" )

		bd.filterTool.size = BigBang.getOptionFloat( "terrain/filter/size" )
		bd.filterTool.functor.index = BigBang.getOptionInt( "terrain/filter/index" )
		
		bd.holeTool.size = BigBang.getOptionFloat( "terrain/cutRepair/size" )
		bd.holeTool.functor.fillNotCut = BigBang.getOptionInt( "terrain/cutRepair/brushMode" )
		return 1

	def render( self, dTime ):
		BigBang.camera().render( dTime )
		BigBang.render( dTime )
		GUI.draw()
		return 1

	#--------------------------------------------------------------------------
	#	Section - Utilities methods
	#--------------------------------------------------------------------------

	def enterChunkVizMode( self ):
		t = BigBang.tool()
		if ( t != None ):
			curr = BigBang.getOptionInt( "render/chunk/vizMode" )
			t.delView( "chunkViz" )
			if curr == 1:
				t.addView( self.chunkViz, "chunkViz" )
			elif curr == 2:
				t.addView( self.vertexViz, "chunkViz" )
			elif curr == 3:
				t.addView( self.meshViz, "chunkViz" )

	def toggleItemSnaps( self ):
		if self.itemSnapMode == 1:
			self.itemSnapMode = 0
		else:
			self.itemSnapMode = 1
		BigBang.setOptionInt( "snaps/itemSnapMode", self.itemSnapMode )
		self.updateItemSnaps()

	def updateItemSnaps( self ):
		#this method calculates itemSnapMode based on the
		#entries in the options.xml
		if ( BigBang.getOptionInt( "snaps/itemSnapMode" ) == 2 ):
			self.itemSnapMode = 2
		elif ( BigBang.getOptionInt( "snaps/itemSnapMode" ) == 1 ):
			self.itemSnapMode = 1
		else:
			self.itemSnapMode = 0
		self.enterItemSnapMode()

	#This method should be called when the item snap type is changed
	def enterItemSnapMode( self ):
		newLoc = None
		t = self.itemTool
		if self.itemSnapMode == 0:
			newLoc = self.itemToolXZLocator
			t.delView( "stdView" )
			t.addView( self.itemToolPlaneView, "stdView" )
			t.size = 1
			BigBang.addCommentaryMsg( "Entering free snap mode" )
		elif self.itemSnapMode == 1:
			newLoc = Locator.TerrainToolLocator()
			t.delView( "stdView" )
			t.addView( self.itemToolTextureView, "stdView" )
			t.size = 1
			BigBang.addCommentaryMsg( "Entering terrain snap mode" )
		elif self.itemSnapMode == 2:
			newLoc = Locator.ChunkObstacleToolLocator()
			t.delView( "stdView" )
			t.addView( self.itemToolModelView, "stdView" )
			t.size = 1
			BigBang.addCommentaryMsg( "Entering obstacle snap mode" )
		else:
			BigBang.addCommentaryMsg( "Unknown snap mode" )

		#finally, recreate the functor
		self.itemTool.locator.subLocator = newLoc

	# Called to change to a given mode, and update the tabs in borland in accordance with this
	def changeToMode( self, modeName ):
		self.currentTab = "tab" + modeName
		self.enterMode( modeName )

	# Called to get the current modeName
	def getMode( self ):
		return self.modeName

	#This method changes the top-level editor mode.
	def enterMode( self, modeName, forceUpdate = 0 ):
	
		#print "enterMode - current %s, new %s, terrainMode %s" % (self.modeName, modeName, self.terrainModeName )
	
		if (self.modeName == modeName) and (not forceUpdate):
			return
			
		t = BigBang.tool()
		if t != None and modeName != "Object":
			if self.itemTool.functor.script.selection.size:
				self.itemTool.functor.script.selection.rem( self.itemTool.functor.script.selection )
				self.itemTool.functor.script.selUpdate()

			BigBang.popTool()

		# Remove the project or height module if we're coming out of project or height mode
		if self.modeName in ("Project", "Height"):
			self.modeStack.pop()
			self.modeName = self.modeStack[ len( self.modeStack ) - 1 ]
			BigBang.pop()

		if ( modeName == "TerrainTexture" ):
			BigBang.pushTool( self.alphaTool )
			self.terrainModeName = modeName

		elif ( modeName == "TerrainHeight" ):
			BigBang.pushTool( self.heightTool )
			self.terrainModeName = modeName

		elif ( modeName == "TerrainFilter" ):
			BigBang.pushTool( self.filterTool )
			self.terrainModeName = modeName

		elif ( modeName == "TerrainHoleCut" ):
			BigBang.pushTool( self.holeTool )
			self.terrainModeName = modeName

		elif ( modeName == "Terrain" ):
			self.modeName = modeName
			self.enterMode( self.terrainModeName )
			self.modeStack.append( modeName )
			return

		elif ( modeName == "Object" ):
			BigBang.pushTool( self.itemTool )
			self.modeStack.append( modeName )

		elif ( modeName == "Project" ):
			BigBang.push( "ProjectModule" )
			self.modeStack.append( modeName )
			
		elif ( modeName == "Height" ):
			BigBang.push( "HeightModule" )
			self.modeStack.append( modeName )

		else:
			BigBang.addCommentaryMsg( "%s mode not yet implemented" % modeName, 1 )

		self.enterChunkVizMode()

		BigBang.addCommentaryMsg( "entered " + modeName + " mode" )
		self.modeName = modeName


	#This method should be called to change the terrain alpha channel.
	def setTerrainAlphaChannel( self, channel ):
		if channel > 3 or channel < 0:
			BigBang.addCommentaryMsg( "Invalid alpha channel " + str(channel), 1 )
		else:
			self.alphaTool.functor.channel = channel


	# This method returns the current channel number
	def getTerrainAlphaChannel( self ):
		return self.alphaTool.functor.channel


	def hasValidTexture( self ):
		return self.alphaTool.functor.texture != BigBang.getOptionString( "resourceGlue/system/notFoundBmp" )


	#This method should be called to change the terrain texture.
	#When a texture is set, the alpha channel previously in use
	#for that channel is looked up and set.
	def setTerrainAlphaTexture( self, textureName ):
		BigBang.addCommentaryMsg( textureName )
		self.alphaTool.functor.texture = textureName


	def setSelection( self, group, update ):
		cif = self.itemTool.functor.script
		set_assign( cif.selection, group )
		if (update == 1):
			cif.selUpdate()
			
	def getSelection( self, group ):
		cif = self.itemTool.functor.script
		set_assign( group, cif.selection )

	# called to reset the editors (something special has changed)
	def resetSelUpdate( self ):
		self.nextTimeDoSelUpdate = 1



def deepCopy( destSect, srcSect ):
	destSect.asString = srcSect.asString
	for x in srcSect.values():
		nd = destSect.createSection( x.name )
		deepCopy( nd, x )
	return destSect


#
# Set functions, for manipulating sets of ChunkItemRevealers
# (ie, what we can select in the editor)
#

# return a copy of a
def set_copy( a ):
	s = BigBang.ChunkItemGroup()
	s.add( a )
	return s

# make a equal to b
def set_assign( a, b ):
	a.rem( a )
	a.add( b )

# remove all elements from a
def set_clear( a ):
	a.rem( a )

# add all the elements in b that aren't in a to a
def set_union( a, b ):
	a.rem( b )
	a.add( b )

# return a new set containing all the elements that are in both a and b
def set_intersection_new( a, b ):
	aonly = set_copy( a )
	aonly.rem( b )

	int = set_copy( a )
	int.rem( aonly )

	return int

# return a new set containing all the elements in a that aren't in b
def set_difference_new( a, b ):
	diff = set_copy( a )
	diff.rem( b )
	return diff

# return true is b is a subset of a
# ie, all of b's elements also exist in a
def set_issubset( a, b ):
	# TODO: JWD 23/9/2003: Shouldn't this be '== b.size'?
	return set_intersection_new( a, b ).size



# This is a helper class for passing state between the module and
#  the object manipulation functor
class ObjInfo:
	def __init__( self ):
		self.overGizmo = 0
		self.shellMode = 0
		self.tabName = ""

	def showBrowse( self ):
		if not self.getBrowsePath() in ["", "unknown file"]:
			BigBang.addCommentaryMsg( self.getBrowsePath() )

	def getBrowsePath( self ):
		#get model file out of the options.xml
		return BigBang.getOptionString( "itemEditor/browsePath" )

	def setBrowsePath( self, name ):
		print "setBrowsePath", name
		BigBang.setOptionString( "itemEditor/browsePath", name )

	def setShellMode( self, state ):
		if self.shellMode == state:
			return
		self.shellMode = state
		
	def setObjectTab( self, tabName ):
		if self.tabName != "":
			# store the current selection
			currentFilter = BigBang.getOptionString( "tools/selectFilter" )
			prevSectionName = "object/" + self.tabName + "/selectFilter"
			BigBang.setOptionString( prevSectionName, currentFilter )
		self.tabName = tabName
		currSectionName = "object/" + self.tabName + "/selectFilter"
		return BigBang.getOptionString( currSectionName )


class ChunkItemFunctor:
	def __init__( self, tool, oi ):
		# set up the tool we are part of
		self.movementLocator = tool.locator
		self.mouseLocator = Locator.ChunkItemLocator(
			tool.locator )
		self.mouseRevealer = self.mouseLocator.revealer
		self.selection = BigBang.ChunkItemGroup()
		
		# leftMouseDown is used to detect if the mouse was pressed inside the 3D
		# view, used when doing a marquee drag select
		
		self.leftMouseDown = 0
		
		self.dragging = 0
		
		self.clickX = 0
		self.clickY = 0
		
		# This const is used when dragOnSelect and when selecting with marquee.
		# This value is relatively big because onMouseEvent's dx and dy parameters
		# are in a sub-pixels scale.
		self.dragStartDelta = 15

		self.mouseView = View.ChunkItemBounds( self.mouseRevealer, 0xff0000ff )
		self.selView = View.ChunkItemBounds( self.selection, 0xff00ff00 )

		tool.locator = self.mouseLocator

		tool.addView( self.selView )
		tool.addView( self.mouseView )

		# store a reference to the object info class
		self.objInfo = oi

		self.oldSelection = BigBang.ChunkItemGroup()
		self.currentSpace_ = ""

	def addChunkItem( self ):
		bp = self.objInfo.getBrowsePath()
		if len(bp)>7 and bp[-7:] == ".prefab":
			self.addChunkPrefab()
		elif self.objInfo.shellMode:
			self.addChunkShell()
		else:
			self.addChunkModel()
			
	def chunkItemAdded( self, name ):
		BigBang.addItemToHistory( self.objInfo.getBrowsePath(), "FILE" );
		BigBang.addCommentaryMsg( "Added " + name )

	def addChunkPrefab( self ):
		group = BigBang.loadChunkPrefab( self.objInfo.getBrowsePath(), self.mouseLocator.subLocator )
		if ( group != None ):
			self.chunkItemAdded( self.objInfo.getBrowsePath() );

	def addChunkModel( self ):
		#If we are adding a model file, then simply
		#add the resource name to the chunk at the locator.
		bp = self.objInfo.getBrowsePath()
		if len(bp)>6 and bp[-6:] == ".model":
			d = ResMgr.DataSection( "model" )
			d.writeString( "resource", bp )
			group = BigBang.createChunkItem( d, self.mouseLocator.subLocator, 2 )
			if ( group != None ):
				self.chunkItemAdded( d.name );
		if len(bp)>4 and bp[-4:] == ".spt":
			d = ResMgr.DataSection( "speedtree" )
			d.writeString( "spt", bp )
			d.writeInt( "seed", 1 )
			group = BigBang.createChunkItem( d, self.mouseLocator.subLocator, 2 )
			if ( group != None ):
				self.chunkItemAdded( d.name );


		# if it's a .xml file in the particles directory, add a ChunkParticles chunk item
		if bp.find("particles/") != -1 and (len(bp)>4 and bp[-4:] == ".xml"):
			d = ResMgr.DataSection( "particles" )
			d.writeString( "resource", bp )
			group = BigBang.createChunkItem( d, self.mouseLocator.subLocator, 2 )
			if ( group != None ):
				self.chunkItemAdded( d.name );

		#XML files represent completely generic additions to the chunk,
		#and so all the information must be deep-copied and added
		#to the chunk.
		elif len(bp)>4 and bp[-4:] == ".xml":
			s = ResMgr.openSection( bp ).values()[0]
			if ( s != None ):
				d = ResMgr.DataSection( s.name )
				deepCopy( d, s )
				group = BigBang.createChunkItem( d, self.mouseLocator.subLocator )
				if ( group != None ):
					self.chunkItemAdded( d.name );

		# If it's a .py file, add an entity with the same name
		elif len(bp)>3 and bp[-3:] == ".py":
			d = ResMgr.DataSection( "entity" )
			d.writeString( "type", bp[len("entities/editor/"):-3] )
			group = BigBang.createChunkItem( d, self.mouseLocator.subLocator )
			if ( group != None ):
				self.chunkItemAdded( d.name );

		# If it's a .def file, add an entity with the same name
		elif len(bp)>4 and bp[-4:] == ".def":
			d = ResMgr.DataSection( "entity" )
			d.writeString( "type", bp[len("entities/defs/"):-4] )
			group = BigBang.createChunkItem( d, self.mouseLocator.subLocator )
			if ( group != None ):
				self.chunkItemAdded( d.name );


	def addChunkShell( self ):

		try:
			(pChunkSection,chunkName) = BigBang.createInsideChunkDataSection()

			if ( pChunkSection == None ):
				BigBang.addCommentaryMsg( "Could not create data section", 1 )
			else:
				# fill in the model information
				pChunkSection.writeString( "model/resource", \
					self.objInfo.getBrowsePath() )

				# open the model data section
				pModelSection = ResMgr.openSection( self.objInfo.getBrowsePath() )
				if ( not pModelSection ):
					BigBang.addCommentaryMsg( \
							"Could not open model file. Shell not added", 1 )
				else:
					# check to see whether the model is nodefull, if so, warn the user
					testSection = pModelSection["nodefullVisual"]
					if testSection != None:
						BigBang.addCommentaryMsg( \
							"warning: model is nodefull, shell not statically lit", 1 )

					# create initial data
					BigBang.chunkFromModel( pChunkSection, pModelSection )

					# copy from the template
					pTemplateSection = ResMgr.openSection( self.objInfo.getBrowsePath() + ".template" )
					if pTemplateSection:
						deepCopy( pChunkSection, pTemplateSection )

					group = BigBang.createChunk( pChunkSection, \
						chunkName, \
						self.mouseLocator.subLocator )

					if ( group != None ):
						self.chunkItemAdded( self.objInfo.getBrowsePath() );

		except Exception, e:
			BigBang.addCommentaryMsg( e.args[0], 1 )

	# key event entry point
	def onKeyEvent( self, (isDown, key, modifiers), tool ):
		if not BigBang.cursorOverGraphicsWnd():
			return 0

		handled = 0
		if not isDown and key == KEY_LEFTMOUSE:
			self.dragging = 0

		if isDown:
			if key == KEY_LEFTMOUSE:
				self.leftMouseDown = 1
				self.onLeftMouse()
				handled = 1
			if key == KEY_MIDDLEMOUSE:
				self.onMiddleMouse()
				handled = 1
			elif key == KEY_RETURN:
				self.addChunkItem()
			elif key == KEY_DELETE:
				if self.selection.size:
					BigBang.deleteChunkItems( self.selection )
			elif key == KEY_ESCAPE:
				# clear the selection
				set_clear( self.selection )
				self.selUpdate()
			#elif key == KEY_R and self.objInfo.shellMode and self.selection.size:
			elif key == KEY_R and self.selection.size:
				BigBang.recreateChunks( self.selection )
				BigBang.addCommentaryMsg( "Recreated chunks" )

				# clear the selection
				set_clear( self.selection )
				self.selUpdate()
#			elif key == KEY_T:
#				BigBang.recalcTerrainShadows()
#				BigBang.addCommentaryMsg( "Recalced shadows" )
			elif key == KEY_C:
				if self.selection.size > 0:
					group = BigBang.cloneChunkItems( self.selection, bd.itemTool.locator.subLocator )

					BigBang.addUndoBarrier( "Clone" )

					BigBang.addCommentaryMsg( "Cloned Selection" )
		else:
			if key == KEY_LEFTMOUSE:
				self.leftMouseDown = 0

		return handled


	# update entry point
	def update( self, dTime, tool ):
		if self.currentSpace_ != BigBang.getOptionString( "space/mru0" ):
			self.currentSpace_ = BigBang.getOptionString( "space/mru0" )
			set_clear( self.selection )
			self.selUpdate()
		if self.objInfo.overGizmo:
			self.mouseView.revealer = None
		else:
			self.mouseView.revealer = self.mouseRevealer

		# TODO: Check if mouse button down and threshold crossed.
		# If so start a drag with the current object
		pass

	def startDragSelect( self ):
		# add a drag select tool, which will pop itself and set our
		# selection when done.
		nt = BigBang.Tool()
		nt.locator = bd.itemTool.locator.subLocator
		nt.functor = Functor.ScriptedFunctor( DragSelectFunctor(nt, self) )
		BigBang.pushTool( nt )
		
	def dragDeltaExceeded( self ):
		return abs( self.clickX ) > self.dragStartDelta or abs( self.clickY ) > self.dragStartDelta

	def onMouseEvent( self, (dx,dy,dz), tool ):
		if dz != 0 \
			and ( BigBang.isKeyDown( KEY_LSHIFT ) \
				  or BigBang.isKeyDown( KEY_RSHIFT ) \
				  or BigBang.getOptionInt( "input/legacyMouseWheel" ) != 0 ) \
			and self.selection.size:
			rotateTool = BigBang.Tool()
			rotateTool.functor = Functor.WheelRotator()
			rotateTool.locator = Locator.OriginLocator()

			rotateTool.handleMouseEvent( dx, dy, dz )

			# Add the mousewheel rotate tool, it'll automatically pop itself
			BigBang.pushTool( rotateTool )
			
		if not BigBang.isKeyDown( KEY_MOUSE0 ):
			# just to make sure that leftMouseDown has a consistent value
			self.leftMouseDown = 0
		
		if self.dragging:
			self.clickX += dx
			self.clickY += dy
			if self.dragDeltaExceeded():
				self.dragging = 0
				try:
					nt = BigBang.Tool()
					nt.locator = bd.itemTool.locator.subLocator
					nt.functor = Functor.MatrixMover()

					#if undoName != None:
					#	nt.functor.setUndoName( undoName )

					# and then push it onto the stack. it'll pop itself off when done
					BigBang.pushTool( nt )
				except ValueError:
					# probably tried to drag a terrain block or something
					# else that can't be moved around.
					pass
		
		elif self.leftMouseDown:
			self.clickX += dx
			self.clickY += dy
			if self.dragDeltaExceeded():
				# not moving through DragOnSelect, so must be a marquee selection.
				self.startDragSelect()
				
		return 0

	def onRightMouse( self ):
		if self.mouseRevealer.size == 1:
			BigBang.rightClick( self.mouseRevealer )
			
	def onLeftMouse( self ):
		self.clickX = 0
		self.clickY = 0
		
		# first see if there's a gizmo in the house
		if self.objInfo.overGizmo:
			# if so, let it take care of things
			BigBang.gizmoClick()
			return

		if not self.mouseRevealer.size:
			# nothing to click on, start a marquee selection. This will take
			# care of clearing the selection if it's only a click.
			self.startDragSelect()
			return

		# Check if control is held down, it indicates that we want to toggle
		# what's pointed to in/out of the selection
		if (BigBang.isKeyDown( KEY_LCONTROL ) or BigBang.isKeyDown( KEY_RCONTROL )):
			# if we're pointing at a subset of the selection
			if set_issubset( self.selection, self.mouseRevealer):
				# remove the subset
				self.selection.rem( self.mouseRevealer )
				self.selUpdate()
				return
			else:
				# add the mouseRevealer to the selection
				set_union( self.selection, self.mouseRevealer )
				self.selUpdate()
				return
		else:
			# if the selection is totally different to what's under the mouse
			# specifically, if we're only pointing at a subset of the
			# selection, we don't want to set the selection to that, we want
			# to drag it instead (which happens below )
			#if not set_intersection_new( self.selection, self.mouseRevealer).size:
			if not set_issubset( self.selection, self.mouseRevealer ):
				# set the selection to what's under the mouse
				set_assign( self.selection, self.mouseRevealer )
				self.selUpdate()



		# nothing under the mouse, bail
		if not self.selection.size:
			return

		if not BigBang.getOptionInt( "dragOnSelect" ):
			if not BigBang.isKeyDown( KEY_V ):
				return


		# ok, it's drag time
		self.dragging = 1


	# The middle mouse button is used for autosnap
	def onMiddleMouse( self ):
		# ensure that we both have a selection and something under the mouse
		if not self.selection.size or not self.mouseRevealer.size:
			return

		# ensure that we're in shell mode
		if not BigBang.isChunkSelected():
			return

		# If v is held down, clone and snap the shell under the cursor
		if BigBang.isKeyDown( KEY_V ):
			group = BigBang.cloneAndAutoSnap( self.mouseRevealer, self.selection )
			if ( group != None ):
				set_assign( self.selection, group )
				self.selUpdate()
			else:
				BigBang.addCommentaryMsg( "No matching portals", 2 )

			return

		# if the selection is different to what's under the mouse,
		if set_difference_new( self.selection, self.mouseRevealer ).size:
			# auto snap the shells together
			if not BigBang.autoSnap( self.selection, self.mouseRevealer ):
				BigBang.addCommentaryMsg( "No matching portals" )

	def selUpdate( self ):
		try:
			# tell big bang what the current selection is
			BigBang.revealSelection( self.selection )

			if self.selection.size:

				self.selEditor = BigBang.ChunkItemEditor( self.selection )

				BigBang.setCurrentEditors( self.selEditor )
				#if hasattr(self.selEditor, "description"):
				#	print "Selected a", str(self.selEditor.description)
				#else:
				#	print "Selected a group"

				# inform the user of stats about the selection
				#if ( self.objInfo.shellMode == 1 and self.selection.size > 1):
				#	BigBang.showChunkReport( self.selection )
			else:
				self.selEditor = None
				BigBang.setCurrentEditors()
		except EnvironmentError, e:
			BigBang.addCommentaryMsg( e.args[0], 1 )			

	def clearAndSaveSelection( self ):
		set_assign( self.oldSelection, self.selection )
		set_clear( self.selection )
		self.selUpdate()
#		BigBang.addCommentaryMsg( "clearAndSaveSelection" )

	def restoreOldSelection( self ):
		set_assign( self.selection, self.oldSelection )
		self.selUpdate()
#		BigBang.addCommentaryMsg( "restoreOldSelection" )



class DragSelectFunctor:
	def __init__( self, tool, cif ):
		# set up the tool we are part of
		self.movementLocator = tool.locator
		self.mouseLocator = Locator.ChunkItemFrustumLocator(
			tool.locator )
		self.mouseRevealer = self.mouseLocator.revealer

		tool.locator = self.mouseLocator
		tool.addView( View.DragBoxView( self.mouseLocator, 0x80202020 ) )
		tool.addView( View.ChunkItemBounds( cif.selection, 0xff00ff00 ) )
		tool.addView( View.ChunkItemBounds( self.mouseRevealer, 0xff0000ff ) )

		self.chunkItemFunctor = cif

	# update entry point
	def update( self, dTime, tool ):
		# must use update to check for this, key events aren't reliable
		if not BigBang.isKeyDown( KEY_LEFTMOUSE ):
			if (BigBang.isKeyDown( KEY_LCONTROL ) or BigBang.isKeyDown( KEY_RCONTROL )):
				# add the set
				set_union( self.chunkItemFunctor.selection, self.mouseRevealer )
			elif BigBang.isKeyDown( KEY_LALT ) or BigBang.isKeyDown( KEY_RALT ):
				# remove the set
				self.chunkItemFunctor.selection.rem( self.mouseRevealer )
			else:
				# set the selection to our mouse revaler
				set_assign( self.chunkItemFunctor.selection, self.mouseRevealer )

			if not BigBang.isKeyDown( KEY_LALT ) and not BigBang.isKeyDown( KEY_RALT ):
				# not removing, so also add whatever is under the cursor
				set_union( self.chunkItemFunctor.selection, self.chunkItemFunctor.mouseRevealer )

			self.chunkItemFunctor.selUpdate()

			BigBang.popTool( )

	def onMouseEvent( self, (dx,dy,dz), tool ):
		return 0

