import ParticleEditorDirector
import ParticleEditor
import ResMgr

from os import startfile

#---------------------------
# The file menu
#---------------------------

def doOpenFile( item ):
    ParticleEditor.openFile()
    
def doSavePS( item ):
    ParticleEditor.savePS()
    
def doReloadTextures( item ):
    ParticleEditor.reloadTextures()    
    
def doExit( item ):
    ParticleEditor.exit()
    
#---------------------------
# The edit menu
#---------------------------    
    
def doUndo( item ):
	ParticleEditor.undo()

def updateUndo( item ):
	return ParticleEditor.canUndo()
  
def doRedo( item ):
	ParticleEditor.redo()
  
def updateRedo( item ):
	return ParticleEditor.canRedo()    
    
#---------------------------
# The view menu
#---------------------------

def doShowToolbar( item ):
    ParticleEditor.showToolbar( "0" )
    
def doHideToolbar( item ):
    ParticleEditor.hideToolbar( "0" )
    
def updateToolbar( item ):
    return ParticleEditor.updateShowToolbar( "0" )
    
def doShowStatusbar( item ):
    ParticleEditor.showStatusbar()  
    
def doHideStatusbar( item ):
    ParticleEditor.hideStatusbar()
    
def updateShowStatusbar( item ):
    return ParticleEditor.updateShowStatusbar()       
    
def doShowPanels( item ):
    ParticleEditor.toggleShowPanels()

def doDefaultPanelLayout( item ):
    ParticleEditor.loadDefaultPanels()
    
def doLoadPanelLayout( item ):
    ParticleEditor.loadRecentPanels()    
                   
#---------------------------
# The help menu
#---------------------------

def doAboutApp( item ):
    ParticleEditor.aboutApp()
    
def doToolsReferenceGuide( item ):
    ParticleEditor.doToolsReferenceGuide()
    
def doContentCreation( item ):
    ParticleEditor.doContentCreation()
    
def doShortcuts( item ):
    ParticleEditor.doShortcuts()

def doRequestFeature( item ):
	startfile( "mailto:support@bigworldtech.com?subject=ParticleEditor %2D Feature Request %2F Bug Report" )
