from FXFileProcessor import FXFileProcessor
from FileUtils import extractSectionsByName

class MFMFileProcessor:
	def __init__( self ):
		self.fxFileProcessor = FXFileProcessor()
		
	def buildDatabase( self, dbEntry ):		
		material = dbEntry.section()
		
		fxs = extractSectionsByName(material,"fx")
		for fx in fxs: dbEntry.addDependency( fx.asString )
		if len(fxs) > 1: dbEntry.flagHasMoreThanOneFX(fxs)
		
	def process( self, dbEntry ):		
		return True
