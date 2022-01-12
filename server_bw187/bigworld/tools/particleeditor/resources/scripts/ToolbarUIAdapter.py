import ParticleEditorDirector
import ParticleEditor
import BigBang
import ResMgr

def doZoomExtents( item ):
	ParticleEditor.zoomToExtents()

def doViewFree( item ):
	ParticleEditor.doViewFree()
    
def updateFreeCamera( item ):
	return ParticleEditor.cameraMode() == 0 

def doViewX( item ):
	ParticleEditor.doViewX()

def updateXCamera( item ):
	return ParticleEditor.cameraMode() == 1
    
def doViewY( item ):
	ParticleEditor.doViewY()
    
def updateYCamera( item ):
	return ParticleEditor.cameraMode() == 2 
    
def doViewZ( item ):
	ParticleEditor.doViewZ()
    
def updateZCamera( item ):
	return ParticleEditor.cameraMode() == 3 
	
def doViewOrbit( item ):
	ParticleEditor.doViewOrbit()
    
def updateOrbitCamera( item ):
	return ParticleEditor.cameraMode() == 4 
	
def updateCamera():
	value = BigBang.getOptionString( "camera/speed" )
	c = ParticleEditor.camera()
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

def doSetBkClr( item ):
	ParticleEditor.doSetBkClr()

def updateBkClr( item ):
	return ParticleEditor.updateBkClr()

def doUndo( item ):
	ParticleEditor.undo()

def updateUndo( item ):
	return ParticleEditor.canUndo()
  
def doRedo( item ):
	ParticleEditor.redo()
  
def updateRedo( item ):
	return ParticleEditor.canRedo()
    
def doPlay( item ):
	ParticleEditor.doPlay()
    
def updateDoPlay( item ):
	return ParticleEditor.getState() == 0
    
def doPause( item ):
	ParticleEditor.doPause()
    
def updateDoPause( item ):
	return ParticleEditor.getState() == 1    
    
def doStop( item ):
	ParticleEditor.doStop()
    
def updateDoStop( item ):
	return ParticleEditor.getState() == 2



