import time

# Utilities for statserver
from turbogears import view

def formatTime( t ):
	if type(t) == long or type(t) == int:
		return time.strftime( "%d %b %Y, %H:%M:%S", time.localtime( int(t) ) )
	elif type(t) == tuple and len(t) == 2:
		return time.strftime( "%d %b %Y, %H:%M:%S", time.localtime( t[0] ) ) + ".%.3d" % (t[1])
	else:
		return t

def inlaySVG(id, svgFilename, width, height, fallbackContentAsXmlString):
		return "<!--[if IE]><script type='text/javascript'>try{new ActiveXObject('Adobe.SVGCtl');document.write('<embed id=\"%s\" width=\"%s\" height=\"%s\" src=\"%s\" class=\"svgex\" />');}catch(e){document.write('%s');}</script><noscript>%s</noscript><![endif]--><!--[if !IE]>--><object id='%s' type='image/svg+xml' width='%s' height='%s' data='%s'>%s</object><!--<![endif]-->" % \
			( id, width, height, svgFilename, fallbackContentAsXmlString, fallbackContentAsXmlString, \
			id, width, height, svgFilename, fallbackContentAsXmlString)

# Function to pass to variableProviders in order to make thigns
# available in the templates
def addVariables( variables ):
	global staticDirectory
	variables['inlaySVG'] = inlaySVG
	variables['formatTime'] = formatTime

def setup():
	view.variable_providers.append( addVariables )
