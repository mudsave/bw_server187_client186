__all__ = ['FlashRemotingGateway']

import pkg_resources
pkg_resources.require("flashticle")

import sys
import traceback
from types import ClassType

from flashticle.remoting import to_remoting, from_remoting
from flashticle.util import StringIO

class FlashRemotingGateway(object):

    def __init__(self, config):
        self.config = config

    def get_processor(self, request):
        if 'DescribeService' in request.headers:
            return NotImplementedError
        return self.process_body

    def get_target(self, target):
        name, meth = target.encode('utf8').replace('/', '.').rsplit('.', 1)
        obj = self.config['services'][name]
        if isinstance(obj, (type, ClassType)):
            obj = obj()
        return getattr(obj, meth)
        
    def get_error_response(self, (cls, e, tb)):
        details = traceback.format_exception(cls, e, tb)
        return dict(
            code='SERVER.PROCESSING',
            level='Error',
            description='%s: %s' % (cls.__name__, e),
            type=cls.__name__,
            details=''.join(details),
        )
    
    def process_body(self, target, response, params):
        func = self.get_target(target)
        try:
            res = func(*params)
        except (SystemExit, KeyboardInterrupt):
            raise
        except:
            res = self.get_error_response(sys.exc_info())
            response += '/onStatus'
        else:
            response += '/onResult'
        return response, 'null', res
        
    def __call__(self, environ, start_response):
        request = from_remoting(environ['wsgi.input'])
        processor = self.get_processor(request)
        io = StringIO()
        body = [processor(*elem) for elem in request.raw_body]
        to_remoting(request.raw_headers, body, io)
        start_response('200 OK', [
            ('Content-Type', 'application/x-amf'),
            ('Content-Length', str(io.tell())),
        ])
        return [io.getvalue()]

class ServiceWrapper(object):
    def __init__(self, module_name, obj_name):
        self._module_name = module_name
        self._obj_name = obj_name

    def __getattr__(self, name):
        if name.startswith('_'):
            raise AttributeError
        print '.'.join([self._module_name, self._obj_name, name])
        parent = self._module_name.rsplit('.', 1)[0]
        mod = __import__(self._module_name, globals(), globals(), [parent])
        obj = getattr(mod, self._obj_name)
        if isinstance(obj, (type, ClassType)):
            obj = obj()
        res = getattr(obj, name)
        print res
        return res

def paste_app(global_conf, **kw):
    from paste.deploy.config import ConfigMiddleware
    from itertools import chain
    conf = {'services': {}}
    services = conf['services']
    for key, value in chain(global_conf.iteritems(), kw.iteritems()):
        if not key.startswith('service '):
            conf[key] = value
            continue
        services[key.split()[1]] = ServiceWrapper(*value.rsplit(':', 1))
    return FlashRemotingGateway(conf)
