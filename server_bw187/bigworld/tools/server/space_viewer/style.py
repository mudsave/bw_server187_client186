# provides an easy to hack around with interface to UI
# stuff in space viewer

import space_viewer

import bwsetup; bwsetup.addPath( ".." )
from pycommon import xmlprefs

prefs = xmlprefs.Prefs( space_viewer.PREFS )

ENTITY_RADIUS = 4
GHOST_ENTITY_RADIUS = 4
ENTITY_SCALE_PERCENT = int( prefs.get( "entityScale" ) or "100" )

# specify colour from entity Types.
entityColours = {
	0: ((111, 114, 247), 2.0), # Player
	1: ((244, 111, 247), 1.0),
	2: ((124, 247, 111), 1.0),
	3: ((114, 241, 244), 1.0),
	4: ((21, 168, 179),  1.0)
}

def getColourForEntity( entityID, entityTypeID ):
	if entityTypeID < len(entityColours) :
		return entityColours[entityTypeID][0]
	else :
		return (255, 192, 192)

def getScaleForEntity( entityID, entityTypeID ):
	if entityTypeID < len(entityColours) :
		return entityColours[entityTypeID][1]
	else :
		return 1.0


def hasAoI( entityID, entityTypeID ):
	return entityTypeID == 0

colourOptions = [
		[ "Cell App ID",         "cellAppIDColour",     "Black" ],
		[ "IP Address",          "ipAddrColour",        "Black" ],
		[ "Cell Load",           "cellLoadColour",      "Gold" ],
		[ "Partition Load",      "partitionLoadColour", None ],
		None,
		[ "Entity Bounds",       "entityBoundsColour",  None ],
		[ "Space Boundary",      "spaceBoundaryColour", "Blue Violet" ],
		[ "Grid",                "gridColour",          "Green" ],
		None,
		[ "Ghost Entity",        "ghostEntityColour",   "Light Grey" ],
		[ "Cell Boundary",       "cellBoundaryColour",  "Blue" ],
	]

colours = [
	None,
	"Black",
	"Blue",
	"Blue Violet",
	"Brown",
	"Cyan",
	"Dark Grey",
	"Dark Green",
	"Gold",
	"Grey",
	"Green",
	"Light Grey",
	"Magenta",
	"Navy",
	"Pink",
	"Red",
	"Sky Blue",
	"Violet",
	"Yellow",
	]

#Sizes of Entities Markers in Percent
sizes = [
	"25",
	"50",
	"75",
	"100",
	"150",
	]

#style.py
