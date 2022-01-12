__all__ = ['load', 'dump', 'loads', 'dumps', 'getname', 'getitems']
import struct
import shutil

import dispatch
from dispatch import strategy, functions, combiners, NoApplicableMethods

from flashticle import amf, util
from flashticle.util import check, StringIO, pack_utf8

class SOLReader(object):
    def __init__(self, f):
        self.f = f
        self.rest = None
        self.length = None

    def read(self, count):
        rval = self.f.read(count)
        if count != 0 and len(rval) == 0:
            raise EOFError
        self.rest -= len(rval)
        return rval
    
    def deserialize(self):
        f = self.f
        check("\x00\xbf", f, "Incorrect header")
        self.rest = self.length = struct.unpack('>I', f.read(4))[0]
        check("TCSO\x00\x04\x00\x00\x00\x00", self, "Incorrect file type")
        rootname = amf.from_amf(amf.TypeCodes.UTF8, self)
        check("\x00\x00\x00\x00", self, "Missing root-name padding")

        root = {}
        while self.rest > 0:
            name = amf.from_amf(amf.TypeCodes.UTF8, self)
            value = amf.from_amf(self.read(1), self)
            check('\x00', self, 'Incorrect root item padding')
            root[name] = value
        return amf.typedobject(rootname, root)

@dispatch.generic()
def getname(obj):
    """
    Get the name of a root object
    """
    pass

@getname.when(strategy.default)
def getname_default(obj):
    return obj.__class__.__name__

@dispatch.generic()
def getitems(obj):
    """
    Return an item iterator for a root object
    """
    pass

@getitems.when(strategy.default)
def getitems_default(obj):
    return getitems(obj.__dict__)

@getitems.when("isinstance(obj, dict)")
def getitems_dict(obj):
    return obj.iteritems()

class SOLWriter(object):
    def __init__(self, f):
        self.f = f
        self.length = 0

    def serialize(self, obj):
        self.f.write('\x00\xbf')
        # length, to be replaced later
        self.f.write('\xDE\xAD\xBE\xEF')
        self.write('TCSO\x00\x04\x00\x00\x00\x00')
        self.write(pack_utf8(getname(obj)))
        self.write('\x00\x00\x00\x00')
        for k, v in getitems(obj):
            self.write(pack_utf8(k))
            amf.write_amf(v, self)
            self.write('\x00')
        self.f.seek(-(self.length + 4), 1)
        self.f.write(struct.pack('>I', self.length))
        self.f.seek(self.length, 1)
        
    def write(self, s):
        self.length += len(s)
        self.f.write(s)

def load(f):
    return SOLReader(f).deserialize()

def loads(s):
    return load(StringIO(s))

def dump(f, obj):
    can_seek = hasattr(f, 'seek') and not getattr(f, 'isatty', lambda: False)()
    if not can_seek:
        sio = StringIO()
        dump(sio, obj)
        shutil.copyfileobj(sio, f)
        return
    SOLWriter(f).serialize(obj)

def dumps(obj):
    sio = StringIO()
    dump(sio, obj)
    return sio.getvalue()
