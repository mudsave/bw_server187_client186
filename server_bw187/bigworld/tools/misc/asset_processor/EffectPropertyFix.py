from LogFile import errorLog
from FileUtils import fullName
from FileUtils import extractSectionsByName
import AssetDatabase

#
# EffectPropertyFix does two things.
# It strips out properties from .fx files and
# places them into .visuals, .mfms or .chunks as needed.
#
class EffectPropertyFix:
	def __init__( self ):
		self.description = "Effect Property Fix"
		
		
	def appliesTo( self ):
		return ("fx","visual","mfm","chunk")
		
		
	def process( self, dbEntry ):		
		name = dbEntry.name
		if ".fx" in name:
			return self.processFx( dbEntry )
		else:
			return self.processMFMorVisual( dbEntry )
		
		
	#Add properties to any sections referring to a .fx file that
	#has alpha test enabled, our double sided
	def processMFMorVisual( self, dbEntry ):
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
				
		fxNames = sect.readStrings( "fx" )
		for fxName in fxNames:			
			fxEntry = AssetDatabase.fileDatabase.get( fxName )
			if hasattr( fxEntry, "alphaTestEnabled"):
				if fxEntry.alphaTestEnabled:					
					propSect = self.newOrExistingPropertySection( sect, "alphaTestEnable" )
					propSect.writeString( "Bool", "true" )
					propSect = self.newOrExistingPropertySection( sect, "alphaReference" )					
					propSect.writeString( "Int", fxEntry.alphaRef )
					changed = True
			if hasattr( fxEntry, "doubleSided" ):
				if fxEntry.doubleSided:
					propSect = self.newOrExistingPropertySection( sect, "doubleSided" )
					propSect.writeString( "Bool", "true" )
					changed = True
							
		return changed
		
	def newOrExistingPropertySection( self, sect, name ):
		for (key,ds) in sect.items():
			if key == "property" or key == "Property":
				if ds.asString == name:
					return ds
		ds = sect.createSection("property")
		ds.asString = name
		return ds		
			
			
	# Strip out any AlphaRef or AlphaTest values from
	# the fx file.
	def processFx( self, dbEntry ):
		output = []
		filename = fullName(dbEntry.name)
		changed = False
		
		try:
			f = open(filename,'r')
		except IOError:		
			errorLog.errorMsg("Could not open %s for reading" % (filename,))
			return False
			
		lines = f.readlines()		
		for line in lines:
			ignore = False
			if "AlphaRef" in line: ignore = True
			if "AlphaTestEnable" in line: ignore = True
			if "AlphaFunc = GREATER" in line: ignore = True
			if "CullMode = CCW;" in line: ignore = True
			if "CullMode = NONE;" in line: ignore = True
			if not ignore:
				output.append(line)
			else:
				changed = True
		f.close()
		
		#output	
		if changed and len(output) > 0:
			f = open(filename,'w+')
			if f == None:
				errorLog.errorMsg("Could not open %s for writing" % (filename,))
				changed = False
			else:
				f.writelines(output)
				f.close()
			
		return changed
			