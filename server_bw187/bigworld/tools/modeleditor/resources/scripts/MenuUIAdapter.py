import ModelEditorDirector
import ModelEditor
import ResMgr

#---------------------------
# The file menu
#---------------------------

def doOpenFile( item ):
	ModelEditor.openFile()
	
def doAddModel( item ):
	ModelEditor.addFile()
	
def doRemoveAddedModels( item ):
	ModelEditor.removeAddedModels()
	
def doRevertModel( item ):
	ModelEditor.revertModel()
	
def hasAddedModels( item ):
	return ModelEditor.hasAddedModels()
	
def doReloadTextures( item ):
	ModelEditor.reloadTextures();
	
def isModelLoaded( item ):
	return ModelEditor.isModelLoaded()
	
def doRegenBoundingBox( item ):
	ModelEditor.regenBoundingBox();
	
def isModelDirty( item ):
	return ModelEditor.isModelDirty()
	
def doSaveFile( item ):
	ModelEditor.saveModel()

#---------------------------
# The edit menu
#---------------------------

def doPrefs( item ):
	ModelEditor.appPrefs()
	
#---------------------------
# The view menu
#---------------------------
	
def doShowAssetLocatorPanel( item ):
	ModelEditor.showPanel( "UAL", 1 )
	
def doShowDisplayPanel( item ):
	ModelEditor.showPanel( "Display", 1 )
	
def doShowObjectPanel( item ):
	ModelEditor.showPanel( "Object", 1 )
	
def doShowAnimationsPanel( item ):
	ModelEditor.showPanel( "Animations", 1 )
	
def doShowActionsPanel( item ):
	ModelEditor.showPanel( "Actions", 1 )

def doShowLODPanel( item ):
	ModelEditor.showPanel( "LOD", 1 )

def doShowLightsPanel( item ):
	ModelEditor.showPanel( "Lights", 1 )

def doShowMaterialsPanel( item ):
	ModelEditor.showPanel( "Materials", 1 )

def doShowMessagesPanel( item ):
	ModelEditor.showPanel( "Messages", 1 )
