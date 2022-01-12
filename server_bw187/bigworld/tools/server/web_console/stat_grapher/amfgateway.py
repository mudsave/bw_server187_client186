# Import standard python libs
from cStringIO import StringIO
import sys
import time
import traceback
import inspect
import cPickle as pickle
import logging

# Setup locally stored library paths
import bwsetup
bwsetup.addPath( "flashticle" )

# Import third party libraries
from flashticle.remoting import to_remoting, from_remoting
from flashticle.amf import to_amf
import turbogears
import cherrypy

# Function decorator which "tags" a function as being exposed
# through the flash AMF gateway
def expose( func ):
	func._exposeAMF = True
	return func

log = logging.getLogger( "stat_grapher.amf" )

# Decorator to allow the use of functions
# This method dispatches calls to the appropriate class methods
class AMFGateway:
	def __init__(self, serviceName = "FlashService"):
		self.amfServiceName = serviceName

	# The main dispatching function
	@turbogears.expose()
	def amfgateway( self ):
		contentType = cherrypy.request.headerMap['Content-Type']
		if contentType != "application/x-amf":
			return self._amfTextResponse()

		try:
			cherrypy.response.headerMap['Content-Type'] = 'application/x-amf'

			log.debug( "=" * 20 )
			log.debug( "amfgateway: Begin handling request" )
			start = time.time()

			amfRequest = from_remoting( cherrypy.request.body )
			fromRemotingTime = time.time()

			log.debug( "amfgateway: Unserialising request took %.3f seconds",
				fromRemotingTime - start )

			headers, body = self._processAMF( amfRequest )
			processTime = time.time()

			log.debug( "amfgateway: "
					"Total method processing time took %.3f seconds",
				processTime - fromRemotingTime )

			out = self._serializeAMF( headers, body )
			serializeTime = time.time()

			log.debug( "amfgateway: Serialising response took %.3f seconds",
				serializeTime - processTime )

			log.debug( "amfgateway: End handling request. Total time was: "
				"%.3f seconds. Size of response: %d bytes.",
				serializeTime - start, len(out) )
			return out
		except Exception, e:
			log.error( "Exception in amfgateway method: %s: %s",
				e.__class__.__name__, e )
			return None

	def _amfTextResponse( self ):
		output = StringIO()
		output.write("""\
<HTML>
	<HEAD>
		<TITLE>AMF Gateway</TITLE>
		<STYLE type="text/css">
		table
		{
			border: 1px solid black;
			border-collapse: collapse;
			width: 50%%;
		}
		td
		{
			padding: 5px;
			border: 1px solid black;
			color: red;
		}
		th
		{
			padding: 5px;
			text-align: left;
		}
		td.funcname
		{
			width: 30%%;
		}
		body {font-family: Verdana}
		</STYLE>
	</HEAD>

<BODY>
<H1>Flash Remoting gateway</H1>
This is the Flash Remoting gateway, which should be accessed via Flash Remoting.<P>
This gateway provides the "<EM>%s</EM>" service.
The following functions are provided by this gateway: <P>""" % \
			(self.amfServiceName))

		methods = inspect.getmembers( self, self.isAMFMethod )
		output.write( "<TABLE>\n" )
		output.write( "<TR><TH>Function name</TH><TH>Parameters</TH></TR>\n" )

		for name, method in methods:
			output.write( "<TR>" )
			output.write( "<TD class=funcname>%s</TD>" % (name) )
			output.write( "<TD>%s</TD>" %
				(", ".join( self.getMethodArgs( method ) )) )
			output.write( "</TR>\n" )
		output.write( "</TABLE> </BODY> </HTML>" )
		return output.getvalue()

	def _processAMF( self, amfRequest ):
		#print amfRequest
		headers = amfRequest.raw_headers
		if 'DescribeService' in amfRequest.headers:
			processor = self._processDescription
		else:
			processor = self._processMethod

		path = cherrypy.request.path
		body = [processor( path, *elem ) for elem in amfRequest.raw_body]
		body = [b for b in body if b != None]
		return headers, body



	def _serializeAMF( self, raw_headers, body ):
		io = StringIO()
		#pickle.dump((raw_headers, body), open('gdata.pickle','w'))
		to_remoting( raw_headers, body, io )
		cherrypy.response.headerMap['Content-Length'] = str( io.tell() )
		return io.getvalue()

	@staticmethod
	def isAMFMethod( func ):
		return inspect.ismethod( func ) and hasattr( func, "_exposeAMF" )

	@staticmethod
	def getMethodArgs( method ):
		args = inspect.getargspec( method )[0]
		if args[0] == "self":
			return args[1:]
		else:
			return args

	def _processDescription( self, path, target, response, params ):
		#print "Path:", path, "Target:", target, "Response", response,
		#	"Params", params

		service = target.split('.')[0]
		if service != self.amfServiceName:
			# Ignore...
			return None

		methods = inspect.getmembers( self, self.isAMFMethod )
		#print methods

		functions= []
		for name, method in methods:
			function = {}
			function["description"] = inspect.getdoc( method )
			function["name"] = name
			function["version"] = "1.0"
			function["returns"]= ""
			functions.append( function )

			arguments = []
			argumentList = self.getMethodArgs( method )
			for arg in argumentList:
				argument = {}
				argument["name"] = arg
				argument["required"] = "true"
				argument["type"] = ""
				arguments.append( argument )
			function["arguments"] = arguments

		res = dict(
				version="1.0",
				address=target,
				functions=functions
				)
		return response + '/onResult', 'null', res

	def _processMethod( self, path, target, response, params ):
		try:
			# Split the path
			targetElems = target.split( '.' )
			if targetElems[0] != self.amfServiceName:
				raise Exception( "Service \"%s\" not provided. "\
					"The only available service is \"%s\"" % \
						(targetElems[0], self.amfServiceName) )

			# Get the target function to call
			curObj = self
			for targetElem in targetElems[1:]:
				curObj = getattr( curObj, targetElem )
			func = curObj
			if not hasattr( func, "_exposeAMF" ):
				raise Exception(
					"This function is not available over Flash Remoting" )

			# Call it!
			log.debug("   Calling remote method: %s", target)
			funcStart = time.time()
			res = func( *params )
			log.debug("   Completed calling remote method: %s (took %.3fs)", target,
				time.time() - funcStart)
			response += '/onResult'

		except Exception, err:
			cls, e, tb = sys.exc_info()
			details = traceback.format_exception( cls, e, tb )
			response += '/onStatus'
			res = dict(
				code = 'SERVER.PROCESSING',
				level = 'Error',
				description = '%s: %s' % (cls.__name__, e),
				type = cls.__name__,
				details = ''.join( details ),
			)
			log.error ( "%s: %s" % (cls.__name__, e) )
			log.error ( ''.join( details ) )

		#print "Path:", path, "Target:", target, "Response", response,
		#	"Params", params
		return response, 'null', res
