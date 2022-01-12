import ModelEditorDirector
import ModelEditor
import BigBang
import ResMgr

#---------------------------
# The main toolbar
#---------------------------

def doUndo( item ):
	ModelEditor.undo()

def updateUndo( item ):
	if (ModelEditor.undo(0) != "" ):
		return 1
	return 0

def doRedo( item ):
	ModelEditor.redo()

def updateRedo( item ):
	if (ModelEditor.redo(0) != "" ):
		return 1
	return 0

def doThumbnail( item ):
	ModelEditor.makeThumbnail()
	
def updateThumbnail( item ):
	return 1
	
def doZoomExtents( item ):
	ModelEditor.zoomToExtents()
	
def doFreeCamera( item ):
	ModelEditor.camera().mode = 0
	
def updateFreeCamera( item ):
	return ModelEditor.camera().mode == 0
	
def doXCamera( item ):
	ModelEditor.camera().mode = 1
	
def updateXCamera( item ):
	return ModelEditor.camera().mode == 1
	
def doYCamera( item ):
	ModelEditor.camera().mode = 2
	
def updateYCamera( item ):
	return ModelEditor.camera().mode == 2
	
def doZCamera( item ):
	ModelEditor.camera().mode = 3
	
def updateZCamera( item ):
	return ModelEditor.camera().mode == 3
	
def doOrbitCamera( item ):
	ModelEditor.camera().mode = 4
	
def updateOrbitCamera( item ):
	return ModelEditor.camera().mode == 4
			
def updateCamera():
	value = BigBang.getOptionString( "camera/speed" )
	c = ModelEditor.camera()
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
	
#---------------------------
# The animations toolbar
#---------------------------
	
def updateLockedAnim( item ):
	return not ModelEditor.isAnimLocked()
	
def doAddAmin( item ):
	ModelEditor.addAnim()
	
def doPlayAnim( item ):
	ModelEditor.playAnim()
	
def updatePlayAnim( item ):
	return not ModelEditor.animPlaying()

def doStopAnim( item ):
	ModelEditor.stopAnim()
	
def updateStopAnim( item ):
	return ModelEditor.animPlaying()

def doLoopAnim( item ):
	ModelEditor.loopAnim()
	
def updateLoopAnim( item ):
	return not ModelEditor.animLooping()

def doRemoveAnim( item ):
	ModelEditor.removeAnim()

#---------------------------
# The compression toolbar
#---------------------------

def doShowOriginalAnim( item ):
	show = BigBang.getOptionInt( "settings/showOriginalAnim", 0 );
	show = not show
	BigBang.setOptionInt( "settings/showOriginalAnim", show );
	
def updateShowOriginalAnim( item ):
	return not BigBang.getOptionInt( "settings/showOriginalAnim", 0 );
	
def doSaveAnim( item ):
	ModelEditor.saveAnimComp()
	
def canSaveAnim( item ):
	return ModelEditor.canSaveAnimComp()

#---------------------------
# The actions toolbar
#---------------------------
	
def updateLockedAct( item ):
	return not ModelEditor.isActLocked()

def doNewAct( item ):
	ModelEditor.newAct()
	
def doRemoveAct( item ):
	ModelEditor.removeAct()
	
def doPromoteAct( item ):
	ModelEditor.promoteAct()
	
def doDemoteAct( item ):
	ModelEditor.demoteAct()
	
#---------------------------
# The LOD toolbar
#---------------------------

def updateLockedLod( item ):
	return not ModelEditor.isLockedLod()

def doNewLod( item ):
	ModelEditor.newLod()
	
def doChangeLodModel( item ):
	ModelEditor.changeLodModel()
	
def updateChangeLodModel( item ):
	return ModelEditor.lodSelected() and not ModelEditor.isFirstLod() and not ModelEditor.isLockedLod() 

def doMoveLodUp( item ):
	ModelEditor.moveLodUp()
	
def updateMoveLodUp( item ):
	return ModelEditor.lodSelected() and ModelEditor.canMoveLodUp() and not ModelEditor.isLockedLod() and not ModelEditor.isMissingLod()

def doMoveLodDown( item ):
	ModelEditor.moveLodDown()
	
def updateMoveLodDown( item ):
	return ModelEditor.lodSelected() and ModelEditor.canMoveLodDown() and not ModelEditor.isLockedLod() and not ModelEditor.isMissingLod()

def doSetLodDist( item ):
	ModelEditor.setLodToDist()

def doExtendLodForever( item ):
	ModelEditor.extendLodForever()
	
def updateLodDist( item ):
	return ModelEditor.lodSelected() and ModelEditor.isFirstLod() or not ModelEditor.isLockedLod() and not ModelEditor.isMissingLod()
	
def doRemoveLod( item ):
	ModelEditor.removeLod()
	
def updateRemoveLod( item ):
	return ModelEditor.lodSelected() and not ModelEditor.isFirstLod() and not ModelEditor.isLockedLod()
	
#---------------------------
# The Lights toolbar
#---------------------------

def doNewLights( item ):
	ModelEditor.newLights()

def doOpenLights( item ):
	ModelEditor.openLights()

def doSaveLights( item ):
	ModelEditor.saveLights()
	
#---------------------------
# The Materials toolbar
#---------------------------

def updateTintSelected( item ):
	return ModelEditor.tintSelected()
	
def doNewTint( item ):
	ModelEditor.newTint()
	
def doLoadMFM( item ):
	ModelEditor.loadMFM()
	
def doSaveMFM( item ):
	ModelEditor.saveMFM()
	
def doDeleteTint( item ):
	ModelEditor.deleteTint()

def updateDeleteTint( item ):
	return ModelEditor.canDeleteTint()