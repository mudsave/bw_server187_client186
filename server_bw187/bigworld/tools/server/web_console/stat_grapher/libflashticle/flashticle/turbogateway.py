__all__ = ['FlashRemotingGateway']

import pkg_resources
pkg_resources.require("flashticle", "TurboGears")

import sys
import traceback

from flashticle.remoting import to_remoting, from_remoting
from flashticle.util import StringIO

import turbogears
import cherrypy
from turbogears import controllers

class FlashRemotingGateway(controllers.Controller):

    @turbogears.expose()
    def index(self):
        contentType = cherrypy.request.headerMap.get('Content-Type', '')
        if contentType != 'application/x-amf':
            return None
        cherrypy.response.headerMap['Content-Type'] = 'application/x-amf'
        self.readAMF()
        headers, body = self.processAMF(cherrypy.request.amfRequest)
        return self.serializeAMF(headers, body)

    def readAMF(self):
        amfRequest = from_remoting(cherrypy.request.body)
        cherrypy.request.amfRequest = amfRequest
        # patch the path. there are only a few options:
        # - 'someurl' + method >> someurl.method
        # - 'someurl/someother' + method >> someurl.someother.method
        if not cherrypy.request.path.endswith('/'):
            cherrypy.request.path += '/'
 
    def processAMF(self, amfRequest):
        print amfRequest
        headers = amfRequest.raw_headers
        if 'DescribeService' in amfRequest.headers:
            processor = self.processDescription
        else:
            processor = self.processBody

        path = cherrypy.request.objectPath or cherrypy.request.path
        body = [processor(path, *elem) for elem in amfRequest.raw_body]
        return headers, body

    def serializeAMF(self, raw_headers, body):
        print raw_headers
        print body
        io = StringIO()
        to_remoting(raw_headers, body, io)
        cherrypy.response.headerMap['Content-Length'] = str(io.tell())
        return io.getvalue()

    def processDescription(self, path, target, response, params):
        raise NotImplementedError

        res = dict(
            version="1.0",
            address=target,
            functions=[
                dict(
                    description="says hello world",
                    name="sayHello",
                    version="1.0",
                    returns="testing",
                ),
            ],
        )
        return response + '/onResult', 'null', res

    def processBody(self, path, target, response, params):
        from cherrypy._cphttptools import mapPathToObject
        targetPath = target.encode('utf8').replace('.', '/')
        path = path.rsplit('/', 1)[0] + '/' + targetPath
        while True:
            try:
                page_handler, object_path, virtual_path = mapPathToObject(path)
                cherrypy.request.objectPath = '/' + '/'.join(object_path[1:])
                args = virtual_path + params
                res = page_handler(*args, **cherrypy.request.paramMap)
                response += '/onResult'
            except cherrypy.InternalRedirect, x:
                path = x.path
                continue
            except (SystemExit, KeyboardInterrupt):
                raise
            except:
                cls, e, tb = sys.exc_info()
                details = traceback.format_exception(cls, e, tb)
                response += '/onStatus'
                res = dict(
                    code='SERVER.PROCESSING',
                    level='Error',
                    description='%s: %s' % (cls.__name__, e),
                    type=cls.__name__,
                    details=''.join(details),
                )
            break
        return response, 'null', res
