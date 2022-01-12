from LogFile import errorLog
from FileUtils import fullName
from FileUtils import extractSectionsByName
import AssetDatabase

#
# Any Individual Asset Processor must have
#
# a list of extensions that the processor applies to
# a process( dbEntry ) method
#
# the process method returns true iff the file was updated.
#

# This class strips out deprecated properties from material sections.
# Just add property names to the stripList and enable the processor in
# the AssetDatabase class
class DeprecatedMaterialPropertyFix:


	def __init__( self ):
		self.description = "Deprecated Material Property Fix"
		self.stripList = []
		self.stripList.append( "diffuseLightExtraModulation" )		
		
		
	def appliesTo( self ):
		return ("model","visual","mfm","chunk")
		
		
	# Strip out any Redundant states from the material section
	def process( self, dbEntry ):				
		dbEntrySection = dbEntry.section()
		changed = False
				
		if ".mfm" in dbEntry.name:
			changed = self.processMaterialSection(dbEntrySection,dbEntry)
		elif ".chunk" in dbEntry.name:			
			models = extractSectionsByName(dbEntrySection,"model")
			for model in models:				
				materials = extractSectionsByName(model,"material")
				for material in materials:										
					changed = self.processMaterialSection(material,dbEntry) or changed			
		else:			
			renderSets = extractSectionsByName(dbEntrySection,"renderSet")
			for rs in renderSets:				
				geometries = extractSectionsByName(rs,"geometry")
				for g in geometries:					
					primitiveGroups = extractSectionsByName(g,"primitiveGroup")
					for pg in primitiveGroups:
						matSect = pg["material"]
						if matSect:
							changed = self.processMaterialSection(matSect,dbEntry) or changed
							
		if changed: dbEntrySection.save()			
		return changed
		
		
	def processMaterialSection( self, sect, dbEntry ):
		changed = False
		validSectionList = []
		sectionNames = []
				
		for (key,value) in sect.items():
			if key == "property" and value.asString in self.stripList:
				changed = True
			else:
				validSectionList.append((key,value))
			sectionNames.append(key)
		
		if changed:
			for key in sectionNames:
				sect.deleteSection(key)
				
			for (key,value) in validSectionList:
				ds = sect.createSection(key)
				ds.copy(value)
							
		return changed