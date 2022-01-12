import os
import sys
x=sys.path
import _AssetProcessor
import ResMgr
sys.path=x
from LogFile import errorLog


#This function is needed because we cannot use ResMgr to save
#out non-xml effect files.  Thus we need to strip out the first
#res path from the BW_RES_PATH environment variable in order to
#save files using the os module and absolute paths.
def findFirstResPath():
	res = os.environ["BW_RES_PATH"]
	idx = res.find(";")
	if idx > 0:
		path = res[0:idx]
	else:
		path = res
	if path[-1] not in ['\\','/']:
		path = path + "\\"
	return path
	
	
#Remove extension from the given filename
def stripExtension(name):
	idx = name.rfind(".")
	if idx != -1:
		return name[:idx]
	return name
	
	
#Return the extension from the given filename
def extension(name):
	try:
		return name[name.rindex(".")+1:]
	except ValueError:
		return ""


#Return the path name (without trailing slash) or return ""
def extractPath(name):
	idx = name.rfind("/")
	if idx == -1:
		idx = name.rfind("\\")
		if idx == -1:
			return ""
	return name[:idx]
	
	
#Return just the filename.
def stripPath(name):
	idx = name.rfind("/")
	if idx == -1:
		idx = name.rfind("\\")
		if idx == -1:
			return name
	return name[idx+1:]
	
	
#Prepend BW_RES_PATH onto the filename
#Note - does not ensure file exists
def fullName(name):
	return BW_RES_PATH + name

		
def extractSectionsByName(sect,name):
	sects = []
	for i in sect.items():
		#print i[0]
		if i[0] == name:
			sects.append(i[1])
	return sects
	
def openSectionFromFolder(filename):
	path = extractPath(filename)
	path = path.replace("\\","/")
	name = stripPath(filename)
	pathSect = ResMgr.openSection(path)
	if pathSect == None:
		errorLog.errorMsg( "Error opening %s" % (path,) )
		return (None,None)
	sect = pathSect[name]
	return (sect,path)
	
	
BW_RES_PATH = findFirstResPath()