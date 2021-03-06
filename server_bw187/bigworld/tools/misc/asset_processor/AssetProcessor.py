import Queue
import FileProvider
import os
import sys
from FileUtils import findFirstResPath
from LogFile import errorLog
import AssetDatabase

#------------------------------------------------------------------------------

#File Processors to import.  Customise this list as you like
fileProcessors = []

#from OverlaidFXTextureStageFix import OverlaidFXTextureStageFix
#fileProcessors.append( OverlaidFXTextureStageFix() )

#from EffectPropertyFix import EffectPropertyFix
#fileProcessors.append( EffectPropertyFix() )

#from RedundantEffectStatesFix import RedundantEffectStatesFix
#fileProcessors.append( RedundantEffectStatesFix() )

#from DeprecatedMaterialProperty import DeprecatedMaterialPropertyFix
#fileProcessors.append( DeprecatedMaterialPropertyFix() )	

from BSP2VersionUpdate import BSP2VersionUpdate
fileProcessors.append( BSP2VersionUpdate() )

#from ChunkFileValidator import ChunkFileValidator
#fileProcessors.append( ChunkFileValidator() )

#------------------------------------------------------------------------------


class AssetProcessor:
	def __init__(self):
		self.showErrors = True
		self.processAllPaths = False
		self.compileDatabaseOnly = False
		self.databaseName = "assetDatabase.xml"
		self.loadDB = False
		
		#check command line args
		n = 1
		while len(sys.argv) > n:
			if not self.parseCmdLineArg(sys.argv[n]):
				self.printUsage()
				return
			n+=1		
				
		self.process()
		
	def parseCmdLineArg( self, arg ):	
		helps = ["/?","?","--help"]		
		if arg in helps:
			self.printUsage
			return 0
	
		suppress = ["-q","/q"]
		if arg in suppress:
			self.showErrors = False
			return 1
			
		compileDBOnly = ["-c","/c"]
		if arg in compileDBOnly:
			self.compileDatabaseOnly = True
			return 1
			
		loadDB = ["-d","/d"]
		if arg in loadDB:
			self.loadDB = True
			return 1
			
		return 0
		
	def printUsage(self):
		print "Usage : python AssetProcessor.py [options]"
		print "  by default the first path from BW_RES_PATH is"
		print "  processed by the asset conversion tool.  This"
		print "  avoids reprocessing the mf/bigworld/res path."
		print ""
		print "Options :"
		print "  -q,/q			: suppress error messages"
		print "  -c,/c			: compile asset database only"
		print "  -d,/d			: load asset database"		
		print "  /?,?,--help     : display this help message"		
		print "AssetProcessor is part of the BigWorld toolkit"
		
	def process(self):
		errorLog.open( "assetProcessor.log",False,not self.showErrors )
		errorLog.infoMsg("Processing %s" % (os.environ["BW_RES_PATH"],))
		errorLog.infoMsg("Writing effects to %s" % (findFirstResPath(),))		
		
		#create the file database
		self.database = AssetDatabase.FileDatabase()
		if self.loadDB:
			self.loadDB = self.database.load(self.databaseName)				
		
		if not self.loadDB:
			self.database.compile()
			
		#process the database
		if not self.compileDatabaseOnly:
			global fileProcessors		
			self.database.process(fileProcessors)
		
		self.database.save(self.databaseName)
		self.database.output()		
		errorLog.save()
		
		if self.showErrors:
			errorLog.output()
		return

		
ap = AssetProcessor()