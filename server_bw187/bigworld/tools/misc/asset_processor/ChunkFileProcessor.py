import sys
x=sys.path
import _AssetProcessor
import ResMgr
sys.path=x
from FileUtils import extractSectionsByName
from FileUtils import stripExtension
from FileUtils import stripPath
from FileUtils import extractPath
from FileUtils import openSectionFromFolder
from LogFile import errorLog		

		
class ChunkFileProcessor:
	def __init__( self ):
		pass
		
	def buildDatabase(self,dbEntry):		
		sect = dbEntry.section()
		models = extractSectionsByName(sect,"model")
		
		for model in models:
			modelName = model.readString( "resource" )
			dbEntry.addDependency( modelName )
			
			materials = extractSectionsByName(model,"material")
			for material in materials:
				fxs = extractSectionsByName(material,"fx")
				for fx in fxs: dbEntry.addDependency( fx.asString )
				if len(fxs) > 1: dbEntry.flagHasMoreThanOneFX(fxs)			
				
	def process(self,dbEntry):
		return True
