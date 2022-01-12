import sys
x=sys.path
import _AssetProcessor
import ResMgr
sys.path=x
from LogFile import errorLog
from FileUtils import extractSectionsByName
from FileUtils import extractPath
from FileUtils import openSectionFromFolder

class ModelFileProcessor:
	def __init__( self ):
		pass
		
	def buildDatabase(self,modelEntry):		
		sect = modelEntry.section()
		
		#ADD VISUAL FROM MODEL
		sects = []
		sects.extend( extractSectionsByName(sect,"nodelessVisual") )
		sects.extend( extractSectionsByName(sect,"nodefullVisual") )
		sects.extend( extractSectionsByName(sect,"billboardVisual") )
		for i in sects:
			name = i.asString + ".visual"
			visual = ResMgr.openSection(name)
			if visual == None:
				name = i.asString + ".static.visual"
				visual = ResMgr.openSection(name)
			
			if visual != None:	
				visualPath = extractPath(name)
				if visualPath != "":					
					visualPath = visualPath.replace("\\","/")
					modelEntry.addDependency( visualPath + "/" + visual.name )					
					
		#ADD MFM FROM DYE
		dyes = extractSectionsByName(sect,"dye")
		for dye in dyes:
			tints = extractSectionsByName(dye,"tint")
			for tint in tints:
				material = tint["material"]
				if material != None:
					mfm = material["mfm"]
					if mfm != None:
						(mfmSect,mfmPath) = openSectionFromFolder(mfm.asString)						
						if mfmSect != None:							
							mfmPath = mfmPath.replace("\\","/")
							modelEntry.addDependency( mfmPath + "/" + mfmSect.name )							
				
	def process(self,dbEntry):
		return True
				