__all__ = [
    'TypeCodes', 'to_amf', 'from_amf', 'write_amf', 'Bag',
    'reference', 'Reference', 'typedobject', 'TypedObject',
]

import struct
import datetime
import time
import weakref
from itertools import imap

import dispatch
from dispatch import strategy, functions, combiners, NoApplicableMethods

from flashticle.util import iter_only, ET, StringIO, pack_utf8

class TypeCodes:
    # http://osflash.org/amf/astypes
    DOUBLE = '\x00'
    BOOL = '\x01'
    UTF8 = '\x02'
    OBJECT = '\x03'
    # Not available in Remoting
    MOVIECLIP = '\x04'
    NULL = '\x05'
    UNDEFINED = '\x06'
    REFERENCE = '\x07'
    MIXEDARRAY = '\x08'
    ENDOFOBJECT = '\x09'
    ARRAY = '\x0a'
    DATE = '\x0b'
    LONGUTF8 = '\x0c'
    UNSUPPORTED = '\x0d'
    # Server-to-client only
    RECORDSET = '\x0e'
    XML = '\x0f'
    TYPEDOBJECT = '\x10'

class EndOfObject(object):
    def __init__(self):
        raise TypeError("singleton")

class Bag(object):
    def __init__(self, dct=None):
        if dct is not None:
            self.__dict__ = dct

    def __getitem__(self, item):
        return self.__dict__[item]

    def __setitem__(self, item, value):
        self.__dict__[item] = value

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.__dict__)

    @dispatch.generic()
    def __cmp__(self, other):
        pass

@Bag.__cmp__.when("isinstance(other, Bag)")
def Bag__cmp__Bag(self, other):
    return cmp(self.__dict__, other.__dict__)

@Bag.__cmp__.when("isinstance(other, dict)")
def Bag__cmp__dict(self, other):
    return cmp(self.__dict__, other)

class Reference(object):
    def __init__(self, refnum):
        self.refnum = refnum

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.refnum)

@dispatch.generic()
def reference(refnum):
    """
    Return an object representing the given reference number
    """
    pass

@reference.when(strategy.default)
def reference_default(refnum):
    return Reference(refnum)

def _read_utf8(f):
    slen = struct.unpack('>H', f.read(2))[0]
    if slen == 0:
        return u''
    return unicode(f.read(slen), 'utf8')

def _to_amf_dict(obj):
    outputAMF = ''
    for k, v in obj.iteritems():
        amfValue = to_amf(v)
        outputAMF += pack_utf8(k) + amfValue
    return outputAMF + '\x00\x00' + TypeCodes.ENDOFOBJECT

def _from_amf_dict(f):
    dct = {}
    while True:
        key = _read_utf8(f)
        value = from_amf(f.read(1), f)
        if key == u'' and value is EndOfObject:
            break
        dct[key] = value
    return dct

class TypedObject(object):
    def __init__(self, dct=None):
        if dct is not None:
            self.__dict__ = dct

    def __cmp__(self, other):
        if isinstance(other, type(self)) or isinstance(self, type(other)):
            return cmp(self.__dict__, other.__dict__)
        raise TypeError

    def __repr__(self):
        return 'typedobject(%r, %r)' % (type(self).__name__, self.__dict__)

TYPEDOBJECT_CLASS_CACHE = weakref.WeakValueDictionary()

@dispatch.generic()
def typedobject(name, dct):
    """
    Return a typed object (class instance) given class name
    and instance dictionary
    """
    pass

@typedobject.when(strategy.default)
def typedobject_default(name, dct):
    if isinstance(name, unicode):
        name = name.encode('utf-8')
    cls = TYPEDOBJECT_CLASS_CACHE.get(name)
    if cls is None:
        cls = type(name, (TypedObject,), {})
        TYPEDOBJECT_CLASS_CACHE[name] = cls
    return cls(dct)


@dispatch.generic()
def from_amf(code, f):
    """
    Given an AMF type code and a file-like object, deserialize it as
    an appropriate Python object
    """
    pass

@from_amf.when(strategy.default)
def from_amf_default(code, f):
    raise TypeError("AMF type code %r can not be deserialized" % (code,))

@dispatch.generic()
def to_amf(obj):
    """
    Serialize an object in AMF format to an iterable that yields strings
    or iterables recursively
    """
    pass

@to_amf.when(strategy.default)
def to_amf_default(obj):
    raise TypeError("%r can not be serialized to AMF" % (obj,))


# DOUBLE

@from_amf.when("code == TypeCodes.DOUBLE")
def from_amf_double(code, f):
    return struct.unpack('>d', f.read(8))[0]

@to_amf.when("isinstance(obj, (float, int, long))")
def to_amf_double(obj):
    return TypeCodes.DOUBLE + struct.pack('>d', obj)

# BOOL

@from_amf.when("code == TypeCodes.BOOL")
def from_amf_bool(code, f):
    return f.read(1) == '\x01'

@to_amf.when("isinstance(obj, bool)")
def to_amf_bool(obj):
    if obj is True:
        return TypeCodes.BOOL + '\x01'
    return TypeCodes.BOOL + '\x00'

# UTF8

@from_amf.when("code == TypeCodes.UTF8")
def from_amf_utf8(code, f):
    return _read_utf8(f)

@to_amf.when("isinstance(obj, str)")
def to_amf_str(obj):
    objlen = len(obj)
    if objlen < 2 ** 16:
        code = TypeCodes.UTF8
        lenword = struct.pack('>H', objlen)
    else:
        code = TypeCodes.LONGUTF8
        lenword = struct.pack('>I', objlen)
    return code + lenword + obj

@to_amf.when("isinstance(obj, unicode)")
def to_amf_unicode(obj):
    return to_amf(obj.encode('utf8'))

# OBJECT

@from_amf.when("code == TypeCodes.OBJECT")
def from_amf_object(code, f):
    return Bag(_from_amf_dict(f))

@to_amf.when("isinstance(obj, Bag)")
def to_amf_bag(obj):
    return TypeCodes.OBJECT + _to_amf_dict(obj.__dict__)

# Not implemented: MOVIECLIP

# NULL and UNDEFINED and UNSUPPORTED

@from_amf.when(
    "code in (TypeCodes.NULL, TypeCodes.UNDEFINED, TypeCodes.UNSUPPORTED)")
def from_amf_null_or_undefined(code, f):
    return None

@to_amf.when("obj is None")
def to_amf_null(obj):
    return TypeCodes.NULL

# REFERENCE

@from_amf.when("code == TypeCodes.REFERENCE")
def from_amf_reference(code, f):
    return reference(struct.unpack('>H', f.read(2)))

@to_amf.when("isinstance(obj, Reference)")
def to_amf(obj):
    return TypeCodes.REFERENCE + struct.pack('>H', obj.refnum)

# MIXEDARRAY

@from_amf.when("code == TypeCodes.MIXEDARRAY")
def from_amf_mixedarray(code, f):
    # skip size, unused
    size = f.read(4)
    return _from_amf_dict(f)

@to_amf.when("isinstance(obj, dict)")
def to_amf_mixedarray(obj):
    #return TypeCodes.MIXEDARRAY, '\x00\x00\x00\x00', _to_amf_dict(obj)
    return TypeCodes.OBJECT + _to_amf_dict(obj)

# ENDOFOBJECT

@from_amf.when("code == TypeCodes.ENDOFOBJECT")
def from_amf_endofobject(code, f):
    return EndOfObject

@to_amf.when("obj is EndOfObject")
def to_amf_endofobject(obj):
    return TypeCodes.ENDOFOBJECT

# ARRAY

@from_amf.when("code == TypeCodes.ARRAY")
def from_amf_array(code, f):
    count = struct.unpack('>I', f.read(4))[0]
    return [from_amf(f.read(1), f) for i in xrange(count)]

@to_amf.when("isinstance(obj, (list, tuple))")
def to_amf_list_tuple(obj):
    return TypeCodes.ARRAY + struct.pack('>I', len(obj)) + \
		''.join( map(to_amf, obj) )

# DATE

@from_amf.when("code == TypeCodes.DATE")
def from_amf_date(code, f):
    ts, tz = struct.unpack('>dh', f.read(10))
    return datetime.datetime.fromtimestamp((ts * 0.001) - (tz * 60))

@to_amf.when("isinstance(obj, datetime.datetime)")
def to_amf_datetime(obj):
    ts = time.mktime(obj.timetuple())
    return TypeCodes.DATE + struct.pack('>d', ts * 1000) + '\x00\x00'

# LONGUTF8

@from_amf.when("code == TypeCodes.LONGUTF8")
def from_amf_longutf8(code, f):
    slen = struct.unpack('>I', f.read(4))
    return unicode(f.read(slen), 'utf8')

# Not Implemented: RECORDSET

# XML

if ET is not None:
    @from_amf.when("code == TypeCodes.XML")
    def from_amf_xml(code, f):
        return ET.ElementTree(ET.XML(from_amf(TypeCodes.LONGUTF8, f)))

    @to_amf.when("isinstance(obj, ET.ElementTree)")
    def to_amf_elementtree(obj):
        f = StringIO()
        obj.write(f, "utf8")
        return XML + struct.pack('>I', s.tell()) + f.getvalue()

# TYPEDOBJECT

@from_amf.when("code == TypeCodes.TYPEDOBJECT")
def from_amf_typedobject(code, f):
    return typedobject(_read_utf8(f), _from_amf_dict(f))

@to_amf.when("isinstance(obj, TypedObject)")
def to_amf_typedobject(obj):
    name = pack_utf8(obj.__class__.__name__)
    return TypeCodes.TYPEDOBJECT + name + _to_amf_dict(obj.__dict__)


def write_amf(obj, f):
    """
    Serialize obj as amf to file-like object f
    """
    iterable = iter_only(to_amf(obj), str)
    writelines = getattr(f, 'writelines', None)
    if writelines is not None:
        writelines(iterable)
    else:
        write = f.write
        for s in iterable:
            write(s)
